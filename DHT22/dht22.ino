#include "DHT.h"

#define DHTPIN 2
#define DHTTYPE DHT22

#define RELAYPIN 4

int maxHum = 60;
int maxTemp = 40;

DHT dht(DHTPIN, DHTTYPE);

void setup()
{
    Serial.begin(115200);
    dht.begin();

    pinMode(RELAYPIN, OUTPUT);
}

void loop()
{
    Serial.print("Test");

    delay(2000);
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if(isnan(h) || isnan(t))
    {
        Serial.println("Failed to read!");
        return;
    }

    Serial.print("H: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("T: ");
    Serial.print(t);
    Serial.println(" *C ");

    if(h > 60.0f)
    {
        digitalWrite(RELAYPIN, HIGH);
    }
    else
    {
        digitalWrite(RELAYPIN, LOW);
    }
}