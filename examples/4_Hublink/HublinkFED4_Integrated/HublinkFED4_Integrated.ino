#include <FED4.h>

FED4 fed4;

void setup()
{
    Serial.begin(9600);
    delay(1000);

    // Enable Hublink functionality
    fed4.useHublink = true;

    // Initialize FED4 (this will also initialize Hublink if enabled)
    fed4.begin("HublinkTest");
}

void loop()
{
    // The run() function now automatically handles Hublink sync
    fed4.run();

    delay(1000);
    Serial.println("I'm alive");
}