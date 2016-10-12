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

void setup()
{
    Serial.begin(9600);
    portDesk.begin(9600);
}

void loop()
{
    if (portDesk.available()) {
        Serial.write(portDesk.read());
        lastMessage = millis();
        isPaused = false;
    }

    if (!isPaused) {
        // TODO Support overflows.
        const int DELAY_MS = 10;
        if (millis() - lastMessage > DELAY_MS) {
            Serial.write(0);
            isPaused = true;
        }
    }

    if (Serial.available() > 0) {
        int incomingByte = Serial.read();
        if (incomingByte == 'U') {
            deskUpDown.setTarget(255);
        } else if (incomingByte == 'D') {
            deskUpDown.setTarget(0);
        }
    }

    deskUpDown.blip();

    /*
	digitalWrite(13, HIGH);   // set the LED on
	delay(500);              // wait for a second
	digitalWrite(13, LOW);    // set the LED off
	delay(500);              // wait for a second
        Serial.write("SARAH\r\n");
    */
}
