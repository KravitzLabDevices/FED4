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

    // Initialize internal RTC from external RTC
    DateTime now = rtc.now();
    Inrtc.setTime(now.unixtime());  

    bool result = false;  // Initialize result variable
    
    // Start preferences session
    if (!preferences.begin(PREFS_NAMESPACE, false))
    {
        Serial.println("Failed to initialize preferences");
        return false;
    }

    // Try-catch like pattern to ensure preferences.end() is always called
    do {
        if (isNewCompilation())
        {
            updateRTC();
            updateCompilationID();
        }
        result = true;  // If we get here, everything succeeded
    } while (false);

    preferences.end();  // Always end the preferences session
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

    Serial.println("RTC time updated successfully");
}

// enables use of fed4.now() when fed4 is instantiated; use rtc.now() internally
DateTime FED4::now()
{
    return rtc.now();
}



/********************************************************
 * Compilation ID Management
 ********************************************************/

String FED4::getCompileDateTime()
{
    static char dateTime[25]; // Static buffer to avoid repeated allocations
    snprintf(dateTime, sizeof(dateTime), "%s %s", __DATE__, __TIME__);
    return String(dateTime);
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
    Serial.println("RTC time adjusted successfully");
}

/**
 * Updates time variables from RTC
 */

void FED4::updateTime(){
  DateTime current = rtc.now();
  currentHour = current.hour(); //useful for timed feeding sessions
  currentMinute = current.minute(); //useful for timed feeding sessions
  currentSecond = current.second(); //useful for timed feeding sessions
  unixtime = current.unixtime();
}