#include "FED4.h"

bool FED4::initializeMagnet()
{
    Serial.println("Initializing magnet sensor");
    if (!magnet.begin_I2C(MLX90393_I2C_ADDRESS, &I2C_2))
    {
        Serial.println("No magnet sensor found... check wiring?");
        return false;
    }

    // Configure default settings
    configureMagnet();
    return true;
}

void FED4::configureMagnet(mlx90393_gain_t gain)
{
    magnet.setGain(gain);
    magnet.setResolution(MLX90393_X, MLX90393_RES_17);
    magnet.setResolution(MLX90393_Y, MLX90393_RES_17);
    magnet.setResolution(MLX90393_Z, MLX90393_RES_16);
    magnet.setOversampling(MLX90393_OSR_3);
    magnet.setFilter(MLX90393_FILTER_5);
}

void FED4::setMagnetGain(mlx90393_gain_t gain)
{
    magnet.setGain(gain);
}

mlx90393_gain_t FED4::getMagnetGain()
{
    return magnet.getGain();
}

bool FED4::readMagnetData(float &x, float &y, float &z)
{
    return magnet.readData(&x, &y, &z);
}

bool FED4::getMagnetEvent(sensors_event_t *event)
{
    return magnet.getEvent(event);
}
