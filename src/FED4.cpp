#include "FED4.h"

// (o)(o)--.
//  \../ (  )
//  m\/m--m'`--.

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : display(SPI_SCK, SPI_MOSI, DISPLAY_CS, 144, 168),
               pixels(NUMPIXELS, STATUS_RGB_LED, NEO_GRB + NEO_KHZ800),
               stepper(STEPS, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4),
               I2C_2(1)
{
    pelletReady = true;
    feedReady = false;
    threshold = 1.1; // changed to percentage of baseline

    // Initialize counters
    pelletCount = 0;
    centerCount = 0;
    leftCount = 0;
    rightCount = 0;
    wakeCount = 0;
    touchTriggers = 0;
}

/********************************************************
 * Initialization
 ********************************************************/
void FED4::begin()
{
    // Initialize serial connection
    Serial.begin(115200);

    // Power up LD02
    pinMode(LDO2_ENABLE, OUTPUT);
    digitalWrite(LDO2_ENABLE, HIGH);

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

    // Initialize and update RTC if needed
    initializeRTC();

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
    mcp.pinMode(EXP_LDO3, OUTPUT);
    mcp.digitalWrite(EXP_LDO3, HIGH);
    mcp.pinMode(EXP_PHOTOGATE_1, INPUT_PULLUP);

    pinMode(AUDIO_TRRS_1, INPUT_PULLUP);
    pinMode(AUDIO_TRRS_2, INPUT);
    pinMode(AUDIO_TRRS_3, INPUT);

    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT);
    pinMode(BUTTON_3, INPUT);

    // Initialize peripherals
    stepper.setSpeed(36);
    pixels.begin();

    // Initialize touch sensors
    touchPadInit();
    // baselineTouchSensors(); // !!only displays, consider removing
    // conceptuually, we should only calibrate once at startup/known conditions
    // or eventually set constants/values in NVS for each device
    calibrateTouchSensors();
    monitorTouchSensors(); // loops if BUTTON_2 (center) is pressed

    // Initialize SD card
    if (!initializeSD())
    {
        Serial.println("SD card initialization failed!");
    }
}