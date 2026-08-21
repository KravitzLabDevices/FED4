# Hublink Integration with FED4

This directory contains examples for using the integrated Hublink functionality with the FED4 library.

## Overview

The FED4 library now includes optional Hublink integration that allows FED4 devices to communicate with the Hublink network for time synchronization and data sharing.

## Features

- **Optional Integration**: Hublink functionality can be excluded from compilation using the `FED4_EXCLUDE_HUBLINK` compiler directive
- **Automatic Time Sync**: RTC is automatically updated when timestamps are received from Hublink
- **Battery Monitoring**: Low battery alerts are automatically sent to Hublink
- **Seamless Integration**: Works with existing FED4 functionality without breaking changes

## Usage

### Basic Usage

```cpp
#include <FED4.h>

FED4 fed4;

void setup() {
    // Enable Hublink functionality
    fed4.useHublink = true;
    
    // Initialize FED4 (Hublink will be initialized automatically)
    fed4.begin("MyProgram");
}

void loop() {
    // Hublink sync happens automatically in run()
    fed4.run();
}
```

### Excluding Hublink from Compilation

If you don't need Hublink functionality, you can exclude it from compilation to reduce code size:

```cpp
#define FED4_EXCLUDE_HUBLINK
#include <FED4.h>

FED4 fed4;

void setup() {
    // Hublink functionality will not be compiled
    fed4.begin("MyProgram");
}
```

### Manual Hublink Control

You can also manually control Hublink synchronization:

```cpp
#include <FED4.h>

FED4 fed4;

void setup() {
    fed4.useHublink = true;
    fed4.begin("MyProgram");
}

void loop() {
    // Manual sync
    fed4.syncHublink();
    
    // Your custom logic here
    delay(1000);
}
```

## Examples

- `HublinkFED4_Integrated/`: Shows how to use the integrated Hublink functionality
- `HublinkFED4/`: Original example showing manual Hublink integration (for reference)

## API Reference

### Properties

- `fed4.useHublink`: Boolean flag to enable/disable Hublink functionality (default: false)

### Methods

- `fed4.initializeHublink()`: Initialize Hublink functionality (called automatically by `begin()`)
- `fed4.syncHublink()`: Sync data with Hublink network
- `fed4.onHublinkTimestampReceived(timestamp)`: Static callback for timestamp reception

### Future Features

**Button-Triggered Sync**: In future versions, you will be able to force Hublink synchronization via button press using the `sync(uint32_t temporaryConnectFor = 0)` function. This allows you to specify a custom duration in seconds for the connection attempt, overriding the default advertising cycling and forcing a sync event.

## Compiler Directives

- `FED4_EXCLUDE_HUBLINK`: Exclude Hublink functionality from compilation

## Requirements

- Hublink library must be installed in your Arduino IDE
- SD card must be properly initialized (Hublink uses SD_CS pin)
- Internet connectivity for Hublink communication

## Documentation

For complete Hublink documentation, configuration options, and advanced usage, visit the [official Hublink documentation](https://hublink.cloud/docs). 