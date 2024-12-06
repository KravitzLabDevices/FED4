#include "FED4.h"

/********************************************************
 * RTC (Real-Time Clock) Management
 *
 * The FED4 device uses a DS3231 RTC to maintain accurate time.
 * On first boot after compilation, the RTC is initialized with
 * the compilation date/time. This allows the device to maintain
 * accurate timestamps even when powered off.
 *
 * The system stores a compilation ID in non-volatile preferences
 * to detect when new code has been uploaded, triggering a RTC
 * update only when needed.
 ********************************************************/

// Debug flag for RTC-related messages
static const bool debugRTC = false;

/********************************************************
 * RTC Functions
 ********************************************************/

void FED4::initializeRTC()
{
    if (debugRTC)
        Serial.println("Initializing RTC...");
    preferences.begin("fed4", false); // Open preferences in RW mode

    if (debugRTC)
    {
        String storedTime = preferences.getString("compileTime", "none");
        Serial.println("Stored compilation time: " + storedTime);
        Serial.println("Current compilation time: " + getCompileDateTime());
    }

    if (isNewCompilation())
    {
        if (debugRTC)
            Serial.println("New compilation detected - updating RTC...");
        updateRTC();
        updateCompilationID();
    }
    else
    {
        if (debugRTC)
            Serial.println("No new compilation detected - maintaining current RTC time");
    }

    // Always show current RTC time
    Serial.print("Current RTC time: ");
    serialPrintRTC();
    Serial.println();
}

void FED4::updateRTC()
{
    String compileDateTime = getCompileDateTime();
    if (debugRTC)
        Serial.println("Updating RTC with compilation time: " + compileDateTime);

    // Parse __DATE__ and __TIME__ strings
    char monthStr[4];
    int month, day, year, hour, minute, second;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
    month = (strstr(month_names, monthStr) - month_names) / 3 + 1;
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    if (debugRTC)
    {
        Serial.printf("Parsed date/time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      year, month, day, hour, minute, second);
    }

    // Update RTC with compilation time
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    if (debugRTC)
    {
        Serial.print("RTC time after update: ");
        serialPrintRTC();
        Serial.println();
    }
}

// enables use of fed4.now() when fed4 is instantiated; use rtc.now() internally
DateTime FED4::now()
{
    return rtc.now();
}

void FED4::serialPrintRTC()
{
    DateTime current = now();
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d",
                  current.year(), current.month(), current.day(),
                  current.hour(), current.minute(), current.second());
}

/********************************************************
 * Compilation ID Management
 ********************************************************/

String FED4::getCompileDateTime()
{
    String dateTime = String(__DATE__) + " " + String(__TIME__);
    return dateTime;
}

bool FED4::isNewCompilation()
{
    String currentCompileTime = getCompileDateTime();
    String storedCompileTime = preferences.getString("compileTime", "");

    if (debugRTC)
    {
        Serial.println("Checking compilation status:");
        Serial.println(" - Stored time: " + storedCompileTime);
        Serial.println(" - Current time: " + currentCompileTime);
        Serial.println(" - Is new compilation? " + String(storedCompileTime != currentCompileTime));
    }

    return (storedCompileTime != currentCompileTime);
}

void FED4::updateCompilationID()
{
    String currentCompileTime = getCompileDateTime();
    preferences.putString("compileTime", currentCompileTime);

    if (debugRTC)
    {
        Serial.println("Updated stored compilation time to: " + currentCompileTime);
        String storedTime = preferences.getString("compileTime", "none");
        Serial.println("Verified stored compilation time: " + storedTime);
    }
}

void FED4::adjustRTC(uint32_t timestamp)
{
    if (debugRTC)
        Serial.println("Adjusting RTC with Unix timestamp: " + String(timestamp));

    rtc.adjust(DateTime(timestamp));

    if (debugRTC)
    {
        Serial.print("RTC time after adjustment: ");
        serialPrintRTC();
        Serial.println();
    }
}