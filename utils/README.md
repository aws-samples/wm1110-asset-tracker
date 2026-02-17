# Asset Tracker Provisioning

The `provision_sidewalk_asset_tracker.py` script automates the creation of the device profile, destination and wireless device certificates in IoT Core. It outputs the UF2 identity file image that can be used to provision asset tracker devices using the MSD UF2 bootloader.

The script will use the local AWS credentials to create the device profile, destination and wireless device identity in IoT Core.

## Prerequisites

- AWS CLI configured with appropriate credentials
- Python 3.8+
- Required Python packages (see requirements.txt)

## Setup

```bash
git clone <insert-repo>
cd wm1110-asset-tracker/utils
```
```bash
# Install the requirements
python3 -m pip install -r requirements.txt
```

## Configuration

Update the `config.yaml` to customize provisioning settings:

```yaml
Config:
    AWS_PROFILE: default              # AWS profile from .aws/credentials
    DESTINATION_NAME: AssetTrackerUplink  # Destination for uplink traffic
    ENABLE_POSITIONING: true          # Enable device positioning for location resolution
    LOCATION_DESTINATION_NAME: AssetTrackerLocation  # Destination for location data (optional)
    HARDWARE_PLATFORM: NORDIC         # NORDIC, TI, SILABS, or ALL
    MFG_ADDRESS: "0xd0000"           # Manufacturing page flash address
```

### Device Positioning

When `ENABLE_POSITIONING` is set to `true`, the provisioning script will:
1. Enable AWS IoT Core Device Location for the Sidewalk device
2. Configure the device to send GNSS/WiFi scan data for location resolution
3. Route resolved location data to the specified destination
4. **Automatically create the location destination if it doesn't exist**

**Important notes about positioning:**
- When positioning is enabled, raw uplink payloads are NOT propagated to the uplink destination
- Location data is resolved by AWS IoT Core and sent to the location destination
- If `LOCATION_DESTINATION_NAME` is not set, it defaults to `DESTINATION_NAME`
- Requires Sidewalk SDK v1.19 or later on the device

### Auto-Created Destinations

The script will automatically create destinations if they don't exist:
- Creates an MQTT topic destination at `sidewalk/<destination_name>`
- Creates the required IAM role with IoT publish permissions
- Works for both uplink and location destinations

To subscribe to location data, use the MQTT topic: `sidewalk/AssetTrackerLocation`

## Usage

```bash
python3 provision_sidewalk_asset_tracker.py
```

Download the resulting `AssetTracker_xxxxx.uf2` file to your local machine, then place the asset tracker device in bootloader mode and copy the file to the bootloader drive to provision the Sidewalk device identity.

### Provisioning Multiple Devices

Multiple identities can be generated with the `-i` option:

```bash
# Provision 5 devices
python3 provision_sidewalk_asset_tracker.py -i 5
```

