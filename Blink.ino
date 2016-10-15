#include <SoftwareSerial.h>

const int rxPin = 12;
const int txPin = 13;  // Not used.
SoftwareSerial portDesk(rxPin, txPin);

bool isPaused = true;
unsigned long lastMessage = 0;

struct DeskUpDown {
    static const int upPin = 7;
    static const int downPin = 6;

    unsigned long lastTime = 0;

    DeskUpDown()
    {
        pinMode(upPin, INPUT);
        pinMode(downPin, INPUT);
    }

    bool setTarget(unsigned target)
    {
        if (lastTime == 0) {
            lastTime = millis();
            if (target == 255) {
                pinMode(upPin, OUTPUT);
            } else if (target == 0) {
                pinMode(downPin, OUTPUT);
            }
            return true;
        } else {
            return false;
        }
    }

    void blip()
    {
        if (lastTime > 0) {
            if (millis() - lastTime > 1000) {
                lastTime = 0;
                pinMode(upPin, INPUT);
                pinMode(downPin, INPUT);
            }
        }
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
        STATE_PRESET_BEFORE_HIT,
        STATE_MOVE_BEFORE_TIMEOUT
    } d_state = STATE_INITIAL;

    enum Trigger {
        TRIGGER_BLIP,
        TRIGGER_TIMEOUT,
        TRIGGER_HEIGHT_HIT,
        TRIGGER_CMD_MOVE,
        TRIGGER_CMD_SET_HEIGHT,
        TRIGGER_CMD_STOP,
        TRIGGER_HEIGHT_CHANGED
    };

    struct CmdMoveArguments {
        int duration;
        bool directionUp;
    };

    void stateTrigger(Trigger tgr, void *data = NULL)
    {
        switch (d_state) {
            case STATE_INITIAL:
            case STATE_PRESET_BEFORE_HIT:
            case STATE_MOVE_BEFORE_TIMEOUT:
                break;
        }
    }

   public:
    int height() const { return d_height; }
    void blip() { stateTrigger(TRIGGER_BLIP); }
    void cmdMove(int duration, bool directionUp)
    {
        CmdMoveArguments args{duration, directionUp};
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
            d_height = msg.height;
            if (msg.height_high == 1) {
                d_height += 256;
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
            deskUpDown.setTarget(255);
        } else if (incomingByte == 'D') {
            deskUpDown.setTarget(0);
        } else if (incomingByte == 'H') {
            char buffer[255];
            sprintf(buffer, "height:%d\r\n", deskState.height());
            Serial.write(buffer);
        }
    }

    deskUpDown.blip();
}
