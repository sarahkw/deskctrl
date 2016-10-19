#include <SoftwareSerial.h>
#include <limits.h>
#include <stdlib.h>
#include "simple-circular-buffer.h"

unsigned long timeBetween(unsigned long from, unsigned long to)
{
    if (to >= from) {
        return to - from;
    }
    // Overflowed.
    return ULONG_MAX - from + to;
}

#define DEBUG_BUFFER

#ifdef DEBUG_BUFFER
CircularBuffer<char, 512> d_debugBuffer;
#endif

const int rxPin = 12;
const int txPin = 13;  // Not used.
SoftwareSerial portDesk(rxPin, txPin);

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
    bool d_returned = false;

    bool d_debug;

    void insert(char byte)
    {
        if (d_position < d_size) {
            d_bytes[d_position++] = byte;
        }
    }

    void clear()
    {
        d_position = 0;
        d_returned = false;
    }

    bool full() const { return d_position == d_size; }
    // No copy or assign.
    ByteCollector(ByteCollector&);
    void operator=(ByteCollector&);

   public:
    ByteCollector(Stream& str, size_t size, bool debug=false)
        : d_stream(str),
          d_bytes(reinterpret_cast<char*>(malloc(size))),
          d_size(size),
          d_debug(debug)
    {
    }
    ~ByteCollector() { free(d_bytes); }
    size_t blip(char* outputBytes)
    {
        if (d_stream.available()) {
            int byte = d_stream.read();
            insert(byte);
#ifdef DEBUG_BUFFER
            if (d_debug) {
                d_debugBuffer.push_back(byte);
            }
#endif
            d_lastMessage = millis();
            d_isPaused = false;
        }

        size_t bytesReturned = 0;

        // Are we full?
        if (full() && !d_returned) {
            d_returned = true;
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

ByteCollector deskByteCollector(portDesk, 4, true);
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
public:
    // how much height has changed before we realize.
    static const int HEIGHT_DELAY = 9;

private:
    int d_height = -1;

    enum class State {
        INITIAL,
        GOTO_HEIGHT,
        MOVE,
        VERIFY_GOTO_HEIGHT,
        FIND_HEIGHT_BLIP,
        FIND_HEIGHT_LISTEN
    } d_state = State::INITIAL;

    enum class Trigger {
        BLIP,
        CMD_MOVE,
        CMD_GOTO_HEIGHT,
        CMD_STOP,
        HEIGHT_UPDATED
    };

    struct CmdGotoHeightData {
        static const int TIMEOUT = 30000;
        unsigned long startedTime;
        int requestedHeight;
        bool directionUp;

        static const int VERIFY_CONVERGE_SKIP = 20;
        static const int VERIFY_CONVERGE_COUNT = 20;
        static const int VERIFY_CONVERGE_TIMEOUT = 10000;
        unsigned long verifyStartedTime;
        int lastHeight;
        int lastHeightSeenCount;
        int skipped;

        bool reachedHeight(int currentHeight) const {
            if (directionUp) {
                if (currentHeight + DeskState::HEIGHT_DELAY > requestedHeight) {
                    return true;
                }
            } else {
                if (currentHeight - DeskState::HEIGHT_DELAY < requestedHeight) {
                    return true;
                }
            }
            return false;
        }
    } d_cmdGotoHeightData;

    struct CmdMoveData {
        unsigned long startedTime;
        unsigned duration;
        bool directionUp;
    } d_cmdMoveData;

    struct CmdFindHeightData {
        // State::FIND_HEIGHT_BLIP
        static const int BLIP_TIMEOUT = 200;
        unsigned long blipStartedTime;
        int targetHeight;

        // State::FIND_HEIGHT_LISTEN
        static const int CONVERGE_COUNT = 5;
        static const int CONVERGE_TIMEOUT = 1000;
        unsigned long listenStartedTime;
        int lastHeight;
        int lastHeightSeenCount;
    } d_cmdFindHeightData;

    template <typename Arg0, typename Arg1>
    struct Arg2 {
        Arg0 arg0;
        Arg1 arg1;
    };

    void stateTrigger(Trigger tgr, void* data = NULL)
    {
        switch (d_state) {
        case State::INITIAL:
            switch (tgr) {
            case Trigger::CMD_MOVE:
                changeState(State::MOVE, data);
                break;
            case Trigger::CMD_GOTO_HEIGHT: {
                int height = *reinterpret_cast<int*>(data);
                if (d_height == -1) {
                    changeState(State::FIND_HEIGHT_BLIP, data);
                } else if (d_height != height) {
                    changeState(State::GOTO_HEIGHT, data);
                } else {
                    // We're already here.
                }
            } break;
            default:
                break;
            }
            break;
        case State::GOTO_HEIGHT:
            switch (tgr) {
            case Trigger::BLIP:
                if (timeBetween(d_cmdGotoHeightData.startedTime, millis()) >
                    CmdGotoHeightData::TIMEOUT) {
                    changeState(State::INITIAL);
                }
                break;
            case Trigger::HEIGHT_UPDATED:
                if (d_cmdGotoHeightData.reachedHeight(d_height)) {
                    changeState(State::VERIFY_GOTO_HEIGHT);
                }
                break;
            default:
                break;
            }
            break;
        case State::MOVE:
            switch (tgr) {
            case Trigger::BLIP:
                if (timeBetween(d_cmdMoveData.startedTime, millis()) >
                    d_cmdMoveData.duration) {
                    changeState(State::INITIAL);
                }
                break;
            default:
                break;
            }
            break;
        case State::VERIFY_GOTO_HEIGHT:
            switch (tgr) {
            case Trigger::HEIGHT_UPDATED: {
                auto& d = d_cmdGotoHeightData;
                if (d.skipped < CmdGotoHeightData::VERIFY_CONVERGE_SKIP) {
                    ++d.skipped;
                    break;
                }
                if (d_height == d.lastHeight) {
                    ++d.lastHeightSeenCount;
                } else {
                    d.lastHeight = d_height;
                    d.lastHeightSeenCount = 1;
                }
                if (d.lastHeightSeenCount ==
                    CmdGotoHeightData::VERIFY_CONVERGE_COUNT) {
                    if (!d_cmdGotoHeightData.reachedHeight(d_height)) {
                        int height = d.requestedHeight;
                        changeState(State::GOTO_HEIGHT, &height);
                    } else {
                        changeState(State::INITIAL);
                    }
                }
            } break;
            case Trigger::BLIP:
                if (timeBetween(d_cmdGotoHeightData.verifyStartedTime,
                                millis()) >
                    CmdGotoHeightData::VERIFY_CONVERGE_TIMEOUT) {
                    changeState(State::INITIAL);
                }
                break;
            default:
                break;
            }
            break;
        case State::FIND_HEIGHT_BLIP:
            switch (tgr) {
            case Trigger::BLIP:
                if (timeBetween(d_cmdFindHeightData.blipStartedTime, millis()) >
                    CmdFindHeightData::BLIP_TIMEOUT) {
                    changeState(State::FIND_HEIGHT_LISTEN);
                }
                break;
            default:
                break;
            }
            break;
        case State::FIND_HEIGHT_LISTEN:
            switch (tgr) {
            case Trigger::HEIGHT_UPDATED: {
                auto& d = d_cmdFindHeightData;
                if (d_height == d.lastHeight) {
                    ++d.lastHeightSeenCount;
                } else {
                    d.lastHeight = d_height;
                    d.lastHeightSeenCount = 1;
                }
                if (d.lastHeightSeenCount ==
                    CmdFindHeightData::CONVERGE_COUNT) {
                    changeState(State::GOTO_HEIGHT, &d.targetHeight);
                }
            } break;
            case Trigger::BLIP:
                if (timeBetween(d_cmdFindHeightData.listenStartedTime,
                                millis()) >
                    CmdFindHeightData::CONVERGE_TIMEOUT) {
                    changeState(State::INITIAL);
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
#ifdef DEBUG_BUFFER
        d_debugBuffer.push_back(0xF << 4 | static_cast<char>(enterState));
#endif
        switch (enterState) {
        case State::INITIAL:
            break;
        case State::GOTO_HEIGHT: {
            int height = *reinterpret_cast<int*>(data);
            d_cmdGotoHeightData.startedTime = millis();
            d_cmdGotoHeightData.requestedHeight = height;
            if (height > d_height) {
                d_cmdGotoHeightData.directionUp = true;
                deskHardware.up();
            } else {
                d_cmdGotoHeightData.directionUp = false;
                deskHardware.down();
            }
        }
            break;
        case State::MOVE: {
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
        case State::VERIFY_GOTO_HEIGHT:
            d_cmdGotoHeightData.verifyStartedTime = millis();
            d_cmdGotoHeightData.lastHeight = -1;
            d_cmdGotoHeightData.lastHeightSeenCount = 0;
            d_cmdGotoHeightData.skipped = 0;
            break;
        case State::FIND_HEIGHT_BLIP: {
            int height = *reinterpret_cast<int*>(data);
            d_cmdFindHeightData.blipStartedTime = millis();
            d_cmdFindHeightData.targetHeight = height;

            // I think Up is probably safer so nobody gets
            // crushed. Haha.
            deskHardware.up();
        } break;
        case State::FIND_HEIGHT_LISTEN:
            d_cmdFindHeightData.listenStartedTime = millis();
            d_cmdFindHeightData.lastHeight = -1;
            d_cmdFindHeightData.lastHeightSeenCount = 0;
            break;
        }
    }

    void stateExit(State exitState)
    {
        switch (exitState) {
        case State::INITIAL:
            break;
        case State::GOTO_HEIGHT:
        case State::MOVE:
            deskHardware.stop();
            break;
        case State::VERIFY_GOTO_HEIGHT:
            break;
        case State::FIND_HEIGHT_BLIP:
            deskHardware.stop();
            break;
        case State::FIND_HEIGHT_LISTEN:
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
    void blip() { stateTrigger(Trigger::BLIP); }
    void cmdMove(int duration, bool directionUp)
    {
        Arg2<int, bool> args{duration, directionUp};
        stateTrigger(Trigger::CMD_MOVE, &args);
    }

    void cmdGotoHeight(int height)
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
            stateTrigger(Trigger::CMD_GOTO_HEIGHT, &height);
        }
    }

    void cmdStop() { stateTrigger(Trigger::CMD_STOP); }
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
            }
            stateTrigger(Trigger::HEIGHT_UPDATED);
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
                deskState.cmdGotoHeight(300);
            } else if (bytes[0] == '2') {
                // 325 = a little too high for sitting
                deskState.cmdGotoHeight(325);
            } else if (bytes[0] == '3') {
                deskState.cmdGotoHeight(418);
            }
#ifdef DEBUG_BUFFER
            else if (bytes[0] == '.') {
                for (auto x = d_debugBuffer.begin(); x != d_debugBuffer.end();
                     x = d_debugBuffer.next(x)) {
                    Serial.write(*x);
                }
                d_debugBuffer.clear();
            }
#endif
        } else if (size == 4) {
            unsigned short command;
            unsigned short argument;
            memcpy(&command, bytes, sizeof(command));
            memcpy(&argument, bytes + 2, sizeof(command));
            
            switch (command) {
            case 1: {
                int height = deskState.height();
                Serial.write(reinterpret_cast<const char*>(&height), 4);
            } break;
            case 2:
                deskState.cmdGotoHeight(argument);
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
