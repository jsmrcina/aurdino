#include <Event.h>
#include <Timer.h>
#include <DHT.h>

// Info for Humidity and Temperature sensor
#define DHT_TYPE DHT22
#define DHT_PIN 12
DHT dht(DHT_PIN, DHT_TYPE);

// Info for relay
#define RELAY_PIN 11

// Info for LED
#define LED_PIN 13

enum class FanState
{
    Off,
    Humidity,
    Timed
};

FanState g_fanState;

// Info for Timers
const int k_readSensorInterval = 2000; // 2 seconds
const long long k_timedEnablementInterval = 28800000; // 8 hours
const long long k_timedDisablementInterval = 600000; // 10 minutes
Timer readSensorTimer;
Timer timedEnablementTimer;
Timer timedDisablementTimer;

void setup()
{
    // Set initial fan state
    g_fanState = FanState::Off;
  
    // Set up DHT module
    dht.begin();

    // Set up relay
    pinMode(RELAY_PIN, OUTPUT);	
    digitalWrite(RELAY_PIN, LOW);

    // Set up timers
    readSensorTimer.every(k_readSensorInterval, checkHumidity);
    timedEnablementTimer.after(k_timedEnablementInterval, timedEnablement);
}

void checkHumidity()
{
    if(g_fanState == FanState::Timed)
    {
        // Ignore humidity measurement while timed
        return;
    }
  
    float h = dht.readHumidity();
    if(isnan(h)) { return; }

    if(h > 70.0f)
    {
        g_fanState = FanState::Humidity;
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
    }
    else
    {
        g_fanState = FanState::Off;
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }
}

void timedEnablement()
{
    g_fanState = FanState::Timed;
    digitalWrite(RELAY_PIN, HIGH);

    // LED Pin should only be on in the humidity enablement case
    digitalWrite(LED_PIN, LOW);
    
    timedDisablementTimer.after(k_timedDisablementInterval, timedDisablement);
}

void timedDisablement()
{
    g_fanState = FanState::Off;
    digitalWrite(RELAY_PIN, LOW);
    timedEnablementTimer.after(k_timedEnablementInterval, timedEnablement);
}

void loop()
{
    readSensorTimer.update();
    timedEnablementTimer.update(); 
    timedDisablementTimer.update();
}
