#include <SoftwareSerial.h>

const int rxPin = 12;
const int txPin = 13; // Not used.
SoftwareSerial portDesk(rxPin, txPin);

bool isPaused = true;
unsigned long lastMessage = 0;

struct DeskUpDown {
    static const int upPin = 7;
    static const int downPin = 6;

    unsigned long lastTime = 0;

    DeskUpDown() {
        pinMode(upPin, INPUT);
        pinMode(downPin, INPUT);
    }

    bool setTarget(unsigned target) {
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

    void blip() {
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
    int position = 0;
    void insert(char byte) {
        if (position < sizeof(bytes)) {
            bytes[position++] = byte;
        }
    }

    void clear() {
        position = 0;
    }

    bool full() const {
        return position == sizeof(bytes);
    }
} byteCollector;

/// Only accept messages if we get two of them in a row. This will
/// help us drop messages that got corrupted when sending.
struct TwiceIsNice {
    char bytes[4];
    bool bytesGood(char test[4]) {
        bool ret = memcmp(test, bytes, 4) == 0;
        memcpy(bytes, test, 4);
        return ret;
    }
} twiceIsNice;

struct DeskState {

  int height = -1;

  void parse(const char *bytes) {
    struct Msg {
      unsigned char a;
      unsigned char b;
      unsigned char height_high;
      unsigned char height;
    };

    const Msg &msg = *reinterpret_cast<const Msg *>(bytes);
    if (msg.a == 1 && msg.b == 1) {
      height = msg.height;
      if (msg.height_high == 1) {
        height += 256;
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
            deskState.parse(byteCollector.bytes);
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
          sprintf(buffer, "height:%d\r\n", deskState.height);
          Serial.write(buffer);
        }
    }

    deskUpDown.blip();
}
