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

/********************************************************
 * RTC Functions
 ********************************************************/

bool FED4::initializeRTC()
{
    // First check if RTC hardware is working
    if (!rtc.begin(&I2C_2))
    {
        return false;
    }

    // Open preferences in read-only mode first
    preferences.begin(PREFS_NAMESPACE, PREFS_RO_MODE);

    if (isNewCompilation())
    {
        // Close read-only mode and reopen in read-write mode
        preferences.end();
        preferences.begin(PREFS_NAMESPACE, PREFS_RW_MODE);

        Serial.println("New compilation detected - updating RTC...");
        updateRTC();
        updateCompilationID();

        // Close read-write mode
        preferences.end();
    }
    else
    {
        // Close read-only mode
        preferences.end();
    }

    return true;
}

void FED4::updateRTC()
{
    String compileDateTime = getCompileDateTime();
    Serial.println("Updating RTC with compilation time: " + compileDateTime);

    // Parse __DATE__ and __TIME__ strings
    char monthStr[4];
    int month, day, year, hour, minute, second;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
    month = (strstr(month_names, monthStr) - month_names) / 3 + 1;
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    Serial.printf("Parsed date/time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);

    // Update RTC with compilation time
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    Serial.print("RTC time after update: ");
    serialPrintRTC();
    Serial.println();
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

    // Serial.println("Checking compilation status:");
    // Serial.println(" - Stored time: " + storedCompileTime);
    // Serial.println(" - Current time: " + currentCompileTime);
    // Serial.println(" - Is new compilation? " + String(storedCompileTime != currentCompileTime));

    return (storedCompileTime != currentCompileTime);
}

void FED4::updateCompilationID()
{
    preferences.begin(PREFS_NAMESPACE, PREFS_RW_MODE);

    String currentCompileTime = getCompileDateTime();
    preferences.putString("compileTime", currentCompileTime);

    Serial.println("Updated stored compilation time to: " + currentCompileTime);
    String storedTime = preferences.getString("compileTime", "none");
    Serial.println("Verified stored compilation time: " + storedTime);

    preferences.end();
}

void FED4::adjustRTC(uint32_t timestamp)
{
    Serial.println("Adjusting RTC with Unix timestamp: " + String(timestamp));
    rtc.adjust(DateTime(timestamp));
    Serial.print("RTC time after adjustment: ");
    serialPrintRTC();
}