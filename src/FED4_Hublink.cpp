#include "FED4.h"

// Static pointer to the current FED4 instance for callback access
static FED4 *currentFED4Instance = nullptr;

/**
 * Initialize Hublink functionality
 * @return true if initialization successful, false otherwise
 */
bool FED4::initializeHublink()
{
#ifndef FED4_EXCLUDE_HUBLINK
    if (!useHublink)
    {
        return false; // Hublink not enabled
    }
    // Store current instance for callback access
    currentFED4Instance = this;

    // Initialize Hublink with SD_CS pin
    if (!hublink.begin())
    {
        Serial.println("Hublink initialization failed");
        return false;
    }

    // Set up the timestamp callback
    hublink.setTimestampCallback(onHublinkTimestampReceived);

    return true;
#else
    return false; // Hublink excluded from compilation
#endif
}

/**
 * Sync Hublink data and handle battery alerts
 */
void FED4::syncHublink()
{
#ifndef FED4_EXCLUDE_HUBLINK
    if (!useHublink)
    {
        return; // Hublink not enabled
    }

    // Check battery level and set alert if low
    int batteryLevel = (int)cellPercent;
    if (batteryLevel < 20)
    {
        hublink.setAlert("Low Battery!");
    }

    // Set battery level before sync
    hublink.setBatteryLevel(batteryLevel);

    // Sync with Hublink (only blocks when ready)
    hublink.sync();
#endif
}

/**
 * Static callback function for Hublink timestamp reception
 * This function is called by Hublink when a timestamp is received
 * @param timestamp Unix timestamp received from Hublink
 */
void FED4::onHublinkTimestampReceived(uint32_t timestamp)
{
#ifndef FED4_EXCLUDE_HUBLINK
    if (currentFED4Instance != nullptr)
    {
        Serial.println("Received timestamp: " + String(timestamp));
        currentFED4Instance->adjustRTC(timestamp);
    }
#endif
}