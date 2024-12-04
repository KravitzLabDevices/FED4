#include "FED4.h"

// (o)(o)--.
//  \../ (  )
//  m\/m--m'`--.

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168),
               pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
               stepper(STEPS, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4),
               I2C_2(1)
{
    pelletReady = true;
    FeedReady = false;
    threshold = 300;

    // Initialize counters
    PelletCount = 0;
    CenterCount = 0;
    LeftCount = 0;
    RightCount = 0;
    WakeCount = 0;
}

/********************************************************
 * Initialization
 ********************************************************/
void FED4::begin()
{
    // Initialize serial connection
    Serial.begin(115200);

    // Power up LD02
    pinMode(POWER_PIN_1, OUTPUT);
    digitalWrite(POWER_PIN_1, HIGH);

    // Initialize primary I2C bus
    if (!Wire.begin())
    {
        Serial.println("I2C Error - check I2C Address");
    }

    // Initialize GPIO expander
    if (!mcp.begin_I2C())
    {
        Serial.println("mcp error");
    }
    Serial.println("mcp ok");

    // Initialize I2C on the second bus
    if (!I2C_2.begin(SDA_2, SCL_2))
    {
        Serial.println("I2C_2 Error - check I2C Address");
        while (1)
            ;
    }

    // Initialize RTC
    if (!rtc.begin(&I2C_2))
    {
        Serial.println("Couldn't find RTC");
    }
    Serial.println("RTC started");

    // Initialize battery monitor
    while (!maxlipo.begin())
    {
        Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!\n"));
    }
    Serial.print(F("Found MAX17048"));
    Serial.print(F(" with Chip ID: 0x"));
    Serial.println(maxlipo.getChipID(), HEX);

    // Initialize temperature/humidity sensor
    if (!aht.begin(&I2C_2))
    {
        Serial.println("Could not find AHT? Check wiring");
        delay(10);
    }
    else
    {
        Serial.println("AHT10 or AHT20 found");
    }

    // Configure GPIO pins
    mcp.pinMode(LDO3, OUTPUT);
    mcp.digitalWrite(LDO3, HIGH);
    mcp.pinMode(PG1, INPUT_PULLUP);
    pinMode(GPIO_PIN_1, INPUT);
    pinMode(GPIO_PIN_2, INPUT);
    pinMode(GPIO_PIN_3, INPUT_PULLUP);

    // Initialize peripherals
    stepper.setSpeed(36);
    pixels.begin();

    // Initialize touch sensors
    touch_pad_init();
    BaselineTouchSensors();
    CalibrateTouchSensors();
}