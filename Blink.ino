#include <SoftwareSerial.h>

const int rxPin = 12;
const int txPin = 13; // Not used.
SoftwareSerial portDesk(rxPin, txPin);

void setup()
{
    Serial.begin(9600);
    portDesk.begin(9600);
}

void loop()
{
    if (portDesk.available()) {
        Serial.write(portDesk.read());
    }

    /*
    if (Serial.available() > 0) {
        int incomingByte = Serial.read();
        Serial.write(incomingByte);
    }
    */

    /*
	digitalWrite(13, HIGH);   // set the LED on
	delay(500);              // wait for a second
	digitalWrite(13, LOW);    // set the LED off
	delay(500);              // wait for a second
        Serial.write("SARAH\r\n");
    */
}
