# Sidewalk Asset Tracker Payload Specifications

This document describes the uplink and downlink message payloads used by the asset tracker.

## Architecture Overview

The asset tracker uses a simplified payload architecture:
- **Location data** is handled by the Sidewalk SDK's `sid_location` API, which manages GNSS/WiFi scanning and cloud-side resolution
- **Sensor telemetry** is sent as a compact 5-byte payload containing battery, temperature, humidity, and motion data

This separation allows the SDK to optimize location scanning and transmission while the application focuses on sensor data.

## Uplink Message Types

| TYPE Value | Name | Description |
| :--: | :--: | :-- |
| 0x01 | SENSOR_TELEMETRY | Sensor data: battery, temperature, humidity, motion |

### SENSOR_TELEMETRY Uplink Message Format (5 bytes)

|      |       |       |       |       |       |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Name** | Type | Battery | Temperature | Humidity | Motion & Accel |
| **Position** | Byte 0 | Byte 1 | Byte 2 | Byte 3 | Byte 4 |

Description:

| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint8_t | Message type in upper 2 bits<br>bit 7-6: TYPE = 0x01 (SENSOR_TELEMETRY)<br>bit 5-0: Reserved<br>*ex. 0x40 = SENSOR_TELEMETRY* |
| 1 | Battery | uint8_t | Battery level as percentage (0-100%)<br>*ex. 0x5A = 90%* |
| 2 | Temperature | int8_t | Temperature in degrees Celsius (signed)<br>*ex. 0x19 = 25°C, 0xF6 = -10°C* |
| 3 | Humidity | uint8_t | Relative humidity as percentage (0-100%)<br>*ex. 0x32 = 50%* |
| 4 | Motion & Accel | uint8_t | bit 7: Motion state (1=in motion, 0=static)<br>bit 6-0: Peak acceleration since last report (0-127)<br>*ex. 0x85 = in motion, peak accel 5* |

### Example Payload

```
Payload: 0x40 0x5A 0x19 0x32 0x85
         │    │    │    │    └── Motion=1, Peak Accel=5
         │    │    │    └─────── Humidity=50%
         │    │    └──────────── Temperature=25°C
         │    └───────────────── Battery=90%
         └────────────────────── Type=SENSOR_TELEMETRY
```

## Location Data

Location data (GNSS and WiFi scan results) is handled entirely by the Sidewalk SDK's `sid_location` API:

- The application calls `sid_location_run()` to trigger a location scan
- The SDK performs GNSS and/or WiFi scanning using the LR1110 radio
- Scan results are automatically transmitted to the Sidewalk cloud
- Cloud-side location resolution converts scan data to coordinates
- Results are delivered to AWS IoT Core for the asset tracker cloud application

This approach provides several benefits:
- Optimized scan parameters managed by the SDK
- Automatic fragmentation for large GNSS payloads
- Built-in retry and error handling
- Consistent location data format across Sidewalk devices

## Downlink Message Types

The asset tracker currently does not process downlink configuration messages. Future versions may add support for:
- Configuration updates (scan intervals, motion thresholds)
- Device commands (factory reset, force scan)

## Migration Notes

This payload format replaces the previous multi-message format (CONFIG, NOLOC, WIFI, GNSS types). Key changes:

1. **Location data removed from application payloads** - Now handled by `sid_location` API
2. **Simplified sensor telemetry** - Single 5-byte message vs. multiple message types
3. **No stored records** - Location data is sent immediately via SDK, not stored locally
4. **No CONFIG uplink** - Device configuration is managed locally

For cloud application compatibility, update your IoT Core rules and Lambda functions to:
- Parse the new SENSOR_TELEMETRY format (type 0x01)
- Receive location data from the Sidewalk Location Service integration
