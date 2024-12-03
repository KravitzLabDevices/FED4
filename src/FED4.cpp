#include "FED4.h"

/********************************************************
 * Constructor
 ********************************************************/
FED4::FED4() : display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 144, 168),
               pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800),
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

/********************************************************
 * Touch Sensor Functions
 ********************************************************/
void IRAM_ATTR FED4::onWakeUp()
{
    // Empty function to serve as interrupt handler for touchpad wake-up
}

void FED4::touch_pad_init()
{
    touch_pad_init();
    touch_pad_config(TOUCH_PIN_1);
    touch_pad_config(TOUCH_PIN_5);
    touch_pad_config(TOUCH_PIN_6);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_filter_start(10);
    delay(200);
}

void FED4::CalibrateTouchSensors()
{
    Serial.println("Touch sensor calibration");
    touchAttachInterrupt(TOUCH_PIN_1, onWakeUp, threshold);
    touchAttachInterrupt(TOUCH_PIN_5, onWakeUp, threshold);
    touchAttachInterrupt(TOUCH_PIN_6, onWakeUp, threshold);
    esp_sleep_enable_touchpad_wakeup();
}

void FED4::BaselineTouchSensors()
{
    Serial.println("Re-baselining...");
    baseline1 = touchRead(TOUCH_PIN_1);
    baseline5 = touchRead(TOUCH_PIN_5);
    baseline6 = touchRead(TOUCH_PIN_6);
    Serial.println("*****");
    Serial.printf("Pin 1: %d\n", baseline1);
    Serial.printf("Pin 5: %d\n", baseline5);
    Serial.printf("Pin 6: %d\n", baseline6);
    Serial.println("*****");
}

void FED4::interpretTouch()
{
    int reading1 = abs((int)(touchRead(TOUCH_PIN_1) - baseline1));
    int reading5 = abs((int)(touchRead(TOUCH_PIN_5) - baseline5));
    int reading6 = abs((int)(touchRead(TOUCH_PIN_6) - baseline6));

    Serial.println("Touch readings (baseline subtracted, abs):");
    Serial.printf("Pin 1: %d\n", reading1);
    Serial.printf("Pin 5: %d\n", reading5);
    Serial.printf("Pin 6: %d\n", reading6);

    int maxReading = 0;
    int triggeredPin = -1;

    if (reading1 > threshold && reading1 > maxReading)
    {
        maxReading = reading1;
        triggeredPin = 1;
    }
    if (reading5 > threshold && reading5 > maxReading)
    {
        maxReading = reading5;
        triggeredPin = 5;
    }
    if (reading6 > threshold && reading6 > maxReading)
    {
        maxReading = reading6;
        triggeredPin = 6;
    }

    switch (triggeredPin)
    {
    case 1:
        Serial.println("Wake-up triggered by Right Poke.");
        RightCount++;
        RedPix();
        break;
    case 5:
        Serial.println("Wake-up triggered by Center Poke.");
        CenterCount++;
        BluePix();
        break;
    case 6:
        Serial.println("Wake-up triggered by Left Poke.");
        GreenPix();
        LeftCount++;
        FeedReady = true;
        break;
    default:
        Serial.println("Wake-up reason unclear, taking no action.");
        WakeCount++;
    }
}

/********************************************************
 * LED Control Functions
 ********************************************************/
void FED4::BluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 20));
    pixels.show();
}

void FED4::DimBluePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 1));
    pixels.show();
}

void FED4::GreenPix()
{
    pixels.setPixelColor(0, pixels.Color(20, 0, 0));
    pixels.show();
}

void FED4::RedPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 20, 0));
    pixels.show();
}

void FED4::PurplePix()
{
    pixels.setPixelColor(0, pixels.Color(0, 50, 50));
    pixels.show();
}

void FED4::NoPix()
{
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    digitalWrite(PIN, LOW);
}

/********************************************************
 * Core Functionality
 ********************************************************/
void FED4::Feed()
{
    if (FeedReady)
    {
        PG1Read = mcp.digitalRead(PG1);

        Serial.println("Feeding!");
        while (PG1Read == 1)
        {                     // while pellet well is empty
            stepper.step(-2); // small movement
            delay(10);
            PG1Read = mcp.digitalRead(PG1);
            pelletReady = true;
        }

        if (pelletReady)
        {
            PelletCount++;
            pelletReady = false;
        }
        FeedReady = false;

        ReleaseMotor();
        SerialOutput();
        strcpy(Event, "PelletDrop");

        // Monitor retrieval
        unsigned long PelletDrop = millis();
        while (PG1Read == 0)
        { // while pellet well is full
            BluePix();
            PG1Read = mcp.digitalRead(PG1);
            RetrievalTime = millis() - PelletDrop;
            if (RetrievalTime > 10000)
                break;
        }
        RedPix();
        strcpy(Event, "PelletTaken");
        RetrievalTime = 0;
    }

    UpdateDisplay();

    // Rebaseline touch sensors every 5 pellets
    if (PelletCount % 5 == 0 && PelletCount > 1)
    {
        BaselineTouchSensors();
    }
}

void FED4::CheckForPellet()
{
    PG1Read = mcp.digitalRead(PG1);
}

void FED4::ReleaseMotor()
{
    digitalWrite(MOTOR_PIN_1, LOW);
    digitalWrite(MOTOR_PIN_2, LOW);
    digitalWrite(MOTOR_PIN_3, LOW);
    digitalWrite(MOTOR_PIN_4, LOW);
}

/********************************************************
 * Display and Output Functions
 ********************************************************/
void FED4::UpdateDisplay()
{
    SerialOutput();

    // Initialize display
    display.begin();
    display.refresh();
    display.setRotation(2);
    display.setTextColor(BLACK);
    display.clearDisplay();

    // Display title
    display.setTextSize(3);
    display.setCursor(12, 20);
    display.print("FED4");
    display.refresh();

    // Switch to smaller text for details
    display.setTextSize(1);

    // Display counts
    display.setCursor(12, 56);
    display.print("Pellets: ");
    display.print(PelletCount);
    display.setCursor(12, 72);
    display.print("L:");
    display.print(LeftCount);
    display.print("   C:");
    display.print(CenterCount);
    display.print("   R:");
    display.print(RightCount);

    // Display environmental data
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    display.setCursor(12, 90);
    display.print("Temp: ");
    display.print(temp.temperature, 1);
    display.print("C");
    display.print(" Hum: ");
    display.print(humidity.relative_humidity, 1);
    display.println("%");

    // Display battery status
    float cellVoltage = maxlipo.cellVoltage();
    float cellPercent = maxlipo.cellPercent();
    display.setCursor(12, 122);
    display.print("Fuel: ");
    display.print(cellVoltage, 1);
    display.print("V, ");
    display.print(cellPercent, 1);
    display.println("%");

    // Display date/time
    DateTime now = rtc.now();
    display.setCursor(12, 140);
    display.print(now.month());
    display.print("/");
    display.print(now.day());
    display.print("/");
    display.print(now.year());
    display.print(" ");
    display.print(now.hour());
    display.print(":");
    if (now.minute() < 10)
        display.print('0');
    display.print(now.minute());

    // Display wake count
    display.setCursor(12, 156);
    display.print("Unclear:");
    display.print(WakeCount);

    display.refresh();
}

void FED4::SerialOutput()
{
    DateTime now = rtc.now();

    // Output date and time
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print("    ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    // Output temperature
    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");
    Serial.println();

    // Output counts
    Serial.print("Pellets: ");
    Serial.print(PelletCount);
    Serial.print(" Center: ");
    Serial.print(CenterCount);
    Serial.print(" Left: ");
    Serial.print(LeftCount);
    Serial.print("  Right: ");
    Serial.println(RightCount);
}

/********************************************************
 * Power Management Functions
 ********************************************************/
void FED4::enterLightSleep()
{
    // Power down
    Serial.println("Powering down...");
    NoPix();
    digitalWrite(POWER_PIN_1, LOW);
    digitalWrite(POWER_PIN_2, LOW);

    // Enter sleep
    Serial.println("Entering light sleep...");
    delay(50);
    esp_light_sleep_start();

    // Wake up
    Serial.println("Woke up!");
    pinMode(POWER_PIN_1, OUTPUT);
    digitalWrite(POWER_PIN_1, HIGH);
    pinMode(POWER_PIN_2, OUTPUT);
    digitalWrite(POWER_PIN_2, HIGH);

    // Post-wake actions
    interpretTouch();
    PurplePix();
    CalibrateTouchSensors();
}

void FED4::Vibrate(unsigned long wait)
{
    mcp.digitalWrite(haptic, HIGH);
    delay(wait);
    mcp.digitalWrite(haptic, LOW);
}