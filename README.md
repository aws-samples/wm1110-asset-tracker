# AWS IoT Asset Tracker Device Application for Amazon Sidewalk 

This repo provides a demonstration asset tracker application using Amazon Sidewalk and AWS IoT Core for the [WioTracker 1110 Dev board](https://www.seeedstudio.com/Wio-Tracker-1110-Dev-Board-p-5799.html) from Seeed Studio. It is designed to work with the [AWS IoT Asset Tracker Demo cloud application](https://github.com/aws-samples/aws-iot-asset-tracker-demo).

For steps to build and deploy the cloud application, please follow this guided workshop: [https://catalog.workshops.aws/sidewalk-asset-tracking/en-US](https://catalog.workshops.aws/sidewalk-asset-tracking/en-US)

## Project Overview

This application gathers onboard sensor data and uses the Sidewalk SDK's location services to send GNSS/WiFi scan data to AWS IoT Core via Amazon Sidewalk. Location data is resolved to geo coordinates using IoT Core Device Location.

Key features:
- Sensor telemetry (battery, temperature, humidity, motion detection)
- Location scanning via Sidewalk SDK's `sid_location` API
- LoRa and BLE connectivity via Amazon Sidewalk
- Low-power operation with configurable scan intervals

## Requirements

- nRF Connect SDK v3.2.1 or later
- Nordic toolchain (installed via nRF Connect for Desktop)
- WioTracker 1110 Dev Board

## Building the Application

### Prerequisites

1. Follow the instructions to Setup the Sidewalk SDK and install the nRF Connect SDK v3.2.1 using [nRF Connect for Desktop](https://docs.nordicsemi.com/bundle/addon-sidewalk-latest/page/setting_up_sidewalk_environment/setting_up_sdk.html)

2. Activate a v3.2.1 shell env:
   ```bash
   nrfutil sdk-manager toolchain launch --ncs-version v3.2.1 --shell
   ```

3. Clone this repository alongside the NCS installation:
   ```bash
   cd /path/to/ncs-workspace
   git clone https://github.com/aws-samples/wm1110-asset-tracker
   ```

### Build Commands

From the `wm1110-asset-tracker` directory:

```bash
# Clean build
west build -b wio_tracker_1110/nrf52840 --pristine -- -DBOARD_ROOT=.

# Incremental build
west build -b wio_tracker_1110/nrf52840 -- -DBOARD_ROOT=.
```

The UF2 image will be at: `build/wm1110-asset-tracker/zephyr/AssetTrackerDeviceApp.uf2`

### Programming

1. Connect the WioTracker 1110 via USB
2. Double-tap the reset button to enter UF2 bootloader mode
3. Copy the UF2 file to the mounted drive

## Device Provisioning

Follow the provisioning tool [instructions](./utils/README.md) to create a Sidewalk identity UF2 image.

## Payload Format

See [PAYLOADS.md](PAYLOADS.md) for the uplink/downlink message specifications.

The application sends:
- **Sensor telemetry**: 5-byte payload with battery, temperature, humidity, and motion data
- **Location data**: Handled automatically by the Sidewalk SDK's `sid_location` API

## Shell Commands

Connect via USB serial (115200 baud) to access the shell:

```
asset-tracker > help
Available commands:
  scan    : Trigger sensor scan and uplink
  status  : Show device status
  config  : Show/set configuration
```

## Configuration

Key Kconfig options in `prj.conf`:

| Option | Description | Default |
|--------|-------------|---------|
| `CONFIG_SIDEWALK_LINK_MASK_LORA` | Enable LoRa link | y |
| `CONFIG_SIDEWALK_SUBGHZ_SUPPORT` | Enable sub-GHz radio | y |
| `CONFIG_SIDEWALK_SUBGHZ_RADIO_LR1110` | Use LR1110 radio | y |
| `CONFIG_IN_MOTION_PER_M` | Motion detection period (minutes) | 5 |
| `CONFIG_MOTION_SCAN_PER_S` | Scan interval when in motion (seconds) | 60 |
| `CONFIG_STATIC_SCAN_PER_M` | Scan interval when static (minutes) | 15 |

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Asset Tracker App                     │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │   Sensors   │  │   Timers    │  │   Shell CLI     │  │
│  │ SHT41/LIS3DH│  │             │  │                 │  │
│  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘  │
│         │                │                  │           │
│         v                v                  v           │
│  ┌──────────────────────────────────────────────────┐   │
│  │              Event Handler (asset_tracker.c)      │   │
│  └──────────────────────────────────────────────────┘   │
│         │                │                              │
│         v                v                              │
│  ┌─────────────┐  ┌─────────────────────────────────┐   │
│  │   Uplink    │  │      sid_location API           │   │
│  │ (telemetry) │  │   (GNSS/WiFi scan & send)       │   │
│  └──────┬──────┘  └──────────────┬──────────────────┘   │
│         │                        │                      │
├─────────┴────────────────────────┴──────────────────────┤
│                    Sidewalk SDK                          │
│              (BLE + LoRa via LR1110)                     │
└─────────────────────────────────────────────────────────┘
```

## Migration from NCS v2.5.0

This version has been updated for NCS v3.2.1 with the following changes:

1. **Board definition**: Migrated to NCS v3.x format (`boards/seeed/wio_tracker_1110/`)
2. **Location services**: Now uses SDK's `sid_location` API instead of custom LR1110 code
3. **Payload format**: Simplified to 5-byte sensor telemetry (location handled by SDK)
4. **No SWDR006 patch**: LR1110 support is built into the Sidewalk SDK

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This library is licensed under the MIT-0 License. See the [LICENSE](LICENSE) file.
