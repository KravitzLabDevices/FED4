#include "FED4.h"

/********************************************************
 * FED4 Accelerometer Functions
 *
 * The FED4 uses a LIS3DH accelerometer on the secondary I2C bus.
 * Default configuration:
 * - Range: ±2G
 * - Data Rate: 50 Hz
 * - Mode: High Resolution (12-bit)
 *
 * Usage Examples:
 *
 * 1. Initialize:
 *    bool success = fed.initializeAccel();
 *
 * 2. Set Range (affects sensitivity):
 *    fed.setAccelRange(LIS3DH_RANGE_2_G);  // ±2g (default)
 *    fed.setAccelRange(LIS3DH_RANGE_4_G);  // ±4g
 *    fed.setAccelRange(LIS3DH_RANGE_8_G);  // ±8g
 *    fed.setAccelRange(LIS3DH_RANGE_16_G); // ±16g
 *
 * 3. Set Performance Mode (affects resolution and power):
 *    fed.setAccelPerformanceMode(LIS3DH_MODE_LOW_POWER);     // 8-bit, lowest power
 *    fed.setAccelPerformanceMode(LIS3DH_MODE_NORMAL);        // 10-bit
 *    fed.setAccelPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION); // 12-bit (default)
 *
 * 4. Set Data Rate (affects power consumption):
 *    fed.setAccelDataRate(LIS3DH_DATARATE_1_HZ);    // 1 Hz
 *    fed.setAccelDataRate(LIS3DH_DATARATE_10_HZ);   // 10 Hz
 *    fed.setAccelDataRate(LIS3DH_DATARATE_25_HZ);   // 25 Hz
 *    fed.setAccelDataRate(LIS3DH_DATARATE_50_HZ);   // 50 Hz (default)
 *    fed.setAccelDataRate(LIS3DH_DATARATE_100_HZ);  // 100 Hz
 *    fed.setAccelDataRate(LIS3DH_DATARATE_200_HZ);  // 200 Hz
 *    fed.setAccelDataRate(LIS3DH_DATARATE_400_HZ);  // 400 Hz
 *
 * 5. Read acceleration data (two methods):
 *    // Method 1: Using sensor event
 *    sensors_event_t event;
 *    if (fed.getAccelEvent(&event)) {
 *        float x = event.acceleration.x;  // m/s^2
 *        float y = event.acceleration.y;  // m/s^2
 *        float z = event.acceleration.z;  // m/s^2
 *    }
 *
 *    // Method 2: Direct reading
 *    float x, y, z;
 *    fed.readAccel(x, y, z);  // values in m/s^2
 *
 * 6. Check for new data:
 *    if (fed.accelDataReady()) {
 *        // New acceleration data available
 *    }
 *
 ********************************************************/

bool FED4::initializeAccel()
{
    // Initialize with default I2C bus
    accel = Adafruit_LIS3DH();

    // Then begin with the default address
    if (!accel.begin(LIS3DH_I2C_ADDRESS)) // Use default I2C address for LIS3DH
    {
        Serial.println("Could not find LIS3DH accelerometer");
        return false;
    }

    // Set default configuration
    accel.setRange(LIS3DH_RANGE_2_G);
    accel.setDataRate(LIS3DH_DATARATE_50_HZ);
    accel.setPerformanceMode(LIS3DH_MODE_HIGH_RESOLUTION);

    Serial.println("LIS3DH accelerometer initialized");
    return true;
}

void FED4::setAccelRange(lis3dh_range_t range)
{
    accel.setRange(range);
}

void FED4::setAccelPerformanceMode(lis3dh_mode_t mode)
{
    accel.setPerformanceMode(mode);
}

void FED4::setAccelDataRate(lis3dh_dataRate_t dataRate)
{
    accel.setDataRate(dataRate);
}

bool FED4::getAccelEvent(sensors_event_t *event)
{
    return accel.getEvent(event);
}

void FED4::readAccel(float &x, float &y, float &z)
{
    sensors_event_t event;
    accel.getEvent(&event);

    x = event.acceleration.x;
    y = event.acceleration.y;
    z = event.acceleration.z;
}

bool FED4::accelDataReady()
{
    return accel.haveNewData();
}
