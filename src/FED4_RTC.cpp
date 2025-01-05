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
    if (!rtc.begin(&I2C_2))
    {
        Serial.println("Couldn't find RTC");
        return false;
    }

    // Set the internal RTC time to the current time
    DateTime now = rtc.now();
    Inrtc.setTime(now.unixtime());  

    // Single preferences session
    if (!preferences.begin(PREFS_NAMESPACE, false))
    {
        Serial.println("Failed to initialize preferences");
        return false;
    }

    bool result = true;
    if (isNewCompilation())
    {
        updateRTC();
        updateCompilationID();
    }

    preferences.end();
    return result;
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
    static String dateTime; // Make static to avoid repeated allocations
    dateTime = String(__DATE__) + " " + String(__TIME__);
    return dateTime;
}

bool FED4::isNewCompilation()
{
    const String currentCompileTime = getCompileDateTime();                    // Use const
    const String storedCompileTime = preferences.getString("compileTime", ""); // Use const
    return (storedCompileTime != currentCompileTime);
}

void FED4::updateCompilationID()
{
    const String currentCompileTime = getCompileDateTime();           // Use const
    preferences.putString("compileTime", currentCompileTime.c_str()); // Use c_str()
}

void FED4::adjustRTC(uint32_t timestamp)
{
    Serial.println("Adjusting RTC with Unix timestamp: " + String(timestamp));
    rtc.adjust(DateTime(timestamp));
    Serial.print("RTC time after adjustment: ");
    serialPrintRTC();
}