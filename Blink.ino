#include <SoftwareSerial.h>

const int rxPin = 12;
const int txPin = 13;  // Not used.
SoftwareSerial portDesk(rxPin, txPin);

bool isPaused = true;
unsigned long lastMessage = 0;

struct DeskUpDown {
    static const int upPin = 7;
    static const int downPin = 6;

    void up()
    {
        pinMode(downPin, INPUT);
        pinMode(upPin, OUTPUT);
    }

    void down()
    {
        pinMode(upPin, INPUT);
        pinMode(downPin, OUTPUT);
    }

    void stop()
    {
        pinMode(upPin, INPUT);
        pinMode(downPin, INPUT);
    }
} deskUpDown;

struct ByteCollector {
    char bytes[4];
    unsigned position = 0;
    void insert(char byte)
    {
        if (position < sizeof(bytes)) {
            bytes[position++] = byte;
        }
    }

    void clear() { position = 0; }
    bool full() const { return position == sizeof(bytes); }
} byteCollector;

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
                static const int TIMEOUT = 10000;
                if (millis() - d_cmdSetHeightData.startedTime > TIMEOUT) {
                    changeState(STATE_INITIAL);
                }
                break;
            case TRIGGER_HEIGHT_CHANGED:
                if (d_cmdSetHeightData.directionUp) {
                    if (d_height > d_cmdSetHeightData.height) {
                        changeState(STATE_INITIAL);
                    }
                } else {
                    if (d_height < d_cmdSetHeightData.height) {
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
                if (millis() - d_cmdMoveData.startedTime > d_cmdMoveData.duration) {
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
                deskUpDown.up();
            } else {
                d_cmdSetHeightData.directionUp = false;
                deskUpDown.down();
            }
        }
            break;
        case STATE_MOVE: {
            Arg2<int, bool>& args = *reinterpret_cast<Arg2<int, bool>*>(data);
            d_cmdMoveData.startedTime = millis();
            d_cmdMoveData.duration = args.arg0;
            d_cmdMoveData.directionUp = args.arg1;
            if (d_cmdMoveData.directionUp) {
                deskUpDown.up();
            } else {
                deskUpDown.down();
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
            deskUpDown.stop();
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
        stateTrigger(TRIGGER_CMD_SET_HEIGHT, &height);
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
    if (portDesk.available()) {
        int byte = portDesk.read();
        // Serial.write(byte);
        byteCollector.insert(byte);
        lastMessage = millis();
        isPaused = false;
    }

    // Are we full?
    if (byteCollector.full()) {
        if (twiceIsNice.bytesGood(byteCollector.bytes)) {
            deskState.parseFromDesk(byteCollector.bytes);
        }
    }

    if (!isPaused) {
        // TODO Support overflows.
        const int DELAY_MS = 10;
        if (millis() - lastMessage > DELAY_MS) {
            // Serial.write(0);
            byteCollector.clear();
            isPaused = true;
        }
    }

    if (Serial.available() > 0) {
        int incomingByte = Serial.read();
        if (incomingByte == 'U') {
            deskState.cmdMove(1000, true);
        } else if (incomingByte == 'D') {
            deskState.cmdMove(1000, false);
        } else if (incomingByte == 'H') {
            char buffer[255];
            sprintf(buffer, "height:%d\r\n", deskState.height());
            Serial.write(buffer);
        } else if (incomingByte == '1') {
            // 300 = height for sitting
            deskState.cmdSetHeight(300);
        } else if (incomingByte == '2') {
            // 325 = a little too high for sitting
            deskState.cmdSetHeight(325);
        } else if (incomingByte == '3') {
            deskState.cmdSetHeight(418);
        }
    }


    deskState.blip();
}
