#include <SoftwareSerial.h>
#include <limits.h>
#include <stdlib.h>

unsigned long timeBetween(unsigned long from, unsigned long to) {
    if (to >= from) {
        return to - from;
    }
    // Overflowed.
    return ULONG_MAX - from + to;
}

const int rxPin = 12;
const int txPin = 13;  // Not used.
SoftwareSerial portDesk(rxPin, txPin);

/// No transistors. This would be great, but the problem is that when
/// the arduino has no power, enough current goes to ground that the
/// desk thinks we press the buttons.
///
// struct DeskHardware {
//     static const int upPin = 7;
//     static const int downPin = 6;
// 
//     void up()
//     {
//         pinMode(downPin, INPUT);
//         pinMode(upPin, OUTPUT);
//     }
// 
//     void down()
//     {
//         pinMode(upPin, INPUT);
//         pinMode(downPin, OUTPUT);
//     }
// 
//     void stop()
//     {
//         pinMode(upPin, INPUT);
//         pinMode(downPin, INPUT);
//     }
// } deskHardware;

struct DeskHardware {
    static const int upPin = 7;
    static const int downPin = 6;

    DeskHardware()
    {
        pinMode(downPin, OUTPUT);
        pinMode(upPin, OUTPUT);
    }

    void up()
    {
        digitalWrite(downPin, LOW);
        digitalWrite(upPin, HIGH);
    }

    void down()
    {
        digitalWrite(upPin, LOW);
        digitalWrite(downPin, HIGH);
    }

    void stop()
    {
        digitalWrite(upPin, LOW);
        digitalWrite(downPin, LOW);
    }
} deskHardware;


class ByteCollector {
    Stream& d_stream;
    char* d_bytes;
    size_t d_size;
    unsigned d_position = 0;

    bool d_isPaused = true;
    unsigned long d_lastMessage = 0;

    void insert(char byte)
    {
        if (d_position < d_size) {
            d_bytes[d_position++] = byte;
        }
    }

    void clear() { d_position = 0; }
    bool full() const { return d_position == d_size; }

    // No copy or assign.
    ByteCollector(ByteCollector&);
    void operator=(ByteCollector&);

   public:
    ByteCollector(Stream& str, size_t size)
        : d_stream(str),
          d_bytes(reinterpret_cast<char*>(malloc(size))),
          d_size(size)
    {
    }
    ~ByteCollector() { free(d_bytes); }
    size_t blip(char* outputBytes)
    {
        if (d_stream.available()) {
            int byte = d_stream.read();
            insert(byte);
            d_lastMessage = millis();
            d_isPaused = false;
        }

        size_t bytesReturned = 0;

        // Are we full?
        if (full()) {
            bytesReturned = d_size;
            memcpy(outputBytes, d_bytes, bytesReturned);
        }

        if (!d_isPaused) {
            const int DELAY_MS = 10;
            if (timeBetween(d_lastMessage, millis()) > DELAY_MS) {
                if (!full()) {
                    bytesReturned = d_position;
                    memcpy(outputBytes, d_bytes, bytesReturned);
                }
                clear();
                d_isPaused = true;
            }
        }

        return bytesReturned;
    }

};

ByteCollector deskByteCollector(portDesk, 4);
ByteCollector controlByteCollector(Serial, 4);

/// Only accept messages if we get two of them in a row. This will
/// help us drop messages that got corrupted when sending.
struct TwiceIsNice {
    char bytes[4];
    bool bytesGood(char test[4])
    {
        bool ret = memcmp(test, bytes, 4) == 0;
        memcpy(bytes, test, 4);
        return ret;
    }
} twiceIsNice;

class DeskState {
    // how much height has changed before we realize.
    static const int HEIGHT_DELAY = 9;

    int d_height = -1;

    enum State {
        STATE_INITIAL,
        STATE_GOTO_HEIGHT,
        STATE_MOVE
    } d_state = STATE_INITIAL;

    enum Trigger {
        TRIGGER_BLIP,
        TRIGGER_CMD_MOVE,
        TRIGGER_CMD_SET_HEIGHT,
        TRIGGER_CMD_STOP,
        TRIGGER_HEIGHT_CHANGED
    };

    struct CmdSetHeightData {
        unsigned long startedTime;
        int height;
        bool directionUp;
    } d_cmdSetHeightData;

    struct CmdMoveData {
        unsigned long startedTime;
        unsigned duration;
        bool directionUp;
    } d_cmdMoveData;

    template <typename Arg0, typename Arg1>
    struct Arg2 {
        Arg0 arg0;
        Arg1 arg1;
    };

    void stateTrigger(Trigger tgr, void* data = NULL)
    {
//        if (tgr == TRIGGER_HEIGHT_CHANGED) {
//            char buffer[255];
//            sprintf(buffer, "New height: %d\r\n", d_height);
//            Serial.write(buffer);
//        }

        switch (d_state) {
        case STATE_INITIAL:
            switch (tgr) {
            case TRIGGER_CMD_MOVE:
                changeState(STATE_MOVE, data);
                break;
            case TRIGGER_CMD_SET_HEIGHT: {
                int height = *reinterpret_cast<int*>(data);
                if (d_height == -1) {
                    // XXX Not ready.
                } else if (d_height != height) {
                    changeState(STATE_GOTO_HEIGHT, data);
                } else {
                    // We're already here.
                }
            } break;
            default:
                break;
            }
            break;
        case STATE_GOTO_HEIGHT:
            switch (tgr) {
            case TRIGGER_BLIP:
                static const int TIMEOUT = 30000;
                if (timeBetween(d_cmdSetHeightData.startedTime, millis()) > TIMEOUT) {
                    changeState(STATE_INITIAL);
                }
                break;
            case TRIGGER_HEIGHT_CHANGED:
                if (d_cmdSetHeightData.directionUp) {
                    if (d_height + HEIGHT_DELAY > d_cmdSetHeightData.height) {
                        changeState(STATE_INITIAL);
                    }
                } else {
                    if (d_height - HEIGHT_DELAY < d_cmdSetHeightData.height) {
                        changeState(STATE_INITIAL);
                    }
                }
                break;
            default:
                break;
            }
            break;
        case STATE_MOVE:
            switch (tgr) {
            case TRIGGER_BLIP:
                if (timeBetween(d_cmdMoveData.startedTime, millis()) >
                    d_cmdMoveData.duration) {
                    changeState(STATE_INITIAL);
                }
                break;
            default:
                break;
            }
            break;
        }
    }

    void stateEnter(State enterState, void* data = NULL)
    {
        switch (enterState) {
        case STATE_INITIAL:
            break;
        case STATE_GOTO_HEIGHT: {
            int height = *reinterpret_cast<int*>(data);
            d_cmdSetHeightData.startedTime = millis();
            d_cmdSetHeightData.height = height;
            if (height > d_height) {
                d_cmdSetHeightData.directionUp = true;
                deskHardware.up();
            } else {
                d_cmdSetHeightData.directionUp = false;
                deskHardware.down();
            }
        }
            break;
        case STATE_MOVE: {
            Arg2<int, bool>& args = *reinterpret_cast<Arg2<int, bool>*>(data);
            d_cmdMoveData.startedTime = millis();
            d_cmdMoveData.duration = args.arg0;
            d_cmdMoveData.directionUp = args.arg1;
            if (d_cmdMoveData.directionUp) {
                deskHardware.up();
            } else {
                deskHardware.down();
            }
        } break;
        }
    }

    void stateExit(State exitState)
    {
        switch (exitState) {
        case STATE_INITIAL:
            break;
        case STATE_GOTO_HEIGHT:
        case STATE_MOVE:
            deskHardware.stop();
            break;
        }
    }

    void changeState(State newState, void* data = NULL)
    {
        stateExit(d_state);
        d_state = newState;
        stateEnter(newState, data);
    }

   public:
    int height() const { return d_height; }
    void blip() { stateTrigger(TRIGGER_BLIP); }
    void cmdMove(int duration, bool directionUp)
    {
        Arg2<int, bool> args{duration, directionUp};
        stateTrigger(TRIGGER_CMD_MOVE, &args);
    }

    void cmdSetHeight(int height)
    {
        if (height == d_height) {
            return;
        }

        const int MIN_HEIGHT = 242;
        const int MAX_HEIGHT = 498;
        if (height < MIN_HEIGHT) {
            height = MIN_HEIGHT;
        } else if (height > MAX_HEIGHT) {
            height = MAX_HEIGHT;
        }

        int heightDiff = height - d_height;
        if (heightDiff < 0) {
            heightDiff = -heightDiff;
        }
        if (heightDiff <= HEIGHT_DELAY) {
            static const int FACTOR = 100;
            cmdMove(heightDiff * FACTOR, height > d_height);
        } else {
            stateTrigger(TRIGGER_CMD_SET_HEIGHT, &height);
        }
    }

    void cmdStop() { stateTrigger(TRIGGER_CMD_STOP); }
    void parseFromDesk(const char *bytes)
    {
        struct Msg {
            unsigned char a;
            unsigned char b;
            unsigned char height_high;
            unsigned char height;
        };

        const Msg &msg = *reinterpret_cast<const Msg *>(bytes);
        if (msg.a == 1 && msg.b == 1) {
            int newHeight = msg.height;
            if (msg.height_high == 1) {
                newHeight += 256;
            }
            if (newHeight != d_height) {
                d_height = newHeight;
                stateTrigger(TRIGGER_HEIGHT_CHANGED);
            }
        }
    }

} deskState;

void setup()
{
    Serial.begin(9600);
    portDesk.begin(9600);
}

void loop()
{
    {
        char bytes[4];
        if (deskByteCollector.blip(bytes) == 4) {
            if (twiceIsNice.bytesGood(bytes)) {
                deskState.parseFromDesk(bytes);
                //            char buf[256];
                //            sprintf(buf, "%x %x %x %x\r\n",
                //            byteCollector.bytes[0],
                //                    byteCollector.bytes[1],
                //                    byteCollector.bytes[2],
                //                    byteCollector.bytes[3]);
                //            Serial.write(buf);
                //            Serial.write(byteCollector.bytes, 4);
            }
        }
    }

    {
        char bytes[4];
        size_t size = controlByteCollector.blip(bytes);
        if (size == 1) {
            if (bytes[0] == 'U') {
                deskState.cmdMove(900, true);
            } else if (bytes[0] == 'D') {
                deskState.cmdMove(900, false);
            } else if (bytes[0] == 'H') {
                char buffer[255];
                sprintf(buffer, "height:%d\r\n", deskState.height());
                Serial.write(buffer);
            } else if (bytes[0] == '1') {
                // 300 = height for sitting
                deskState.cmdSetHeight(300);
            } else if (bytes[0] == '2') {
                // 325 = a little too high for sitting
                deskState.cmdSetHeight(325);
            } else if (bytes[0] == '3') {
                deskState.cmdSetHeight(418);
            }
        } else if (size == 4) {
            unsigned short command;
            unsigned short argument;
            memcpy(&command, bytes, sizeof(command));
            memcpy(&argument, bytes + 2, sizeof(command));
            
            switch (command) {
            case 2:
                deskState.cmdSetHeight(argument);
                break;
            case 3:
                deskState.cmdMove(argument, true);
                break;
            case 4:
                deskState.cmdMove(argument, false);
                break;
            default:
                break;
            }
        }
    }

    deskState.blip();
}
