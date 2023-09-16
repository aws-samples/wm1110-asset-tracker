# Asset Tracker Provisioning

The ~provision_sidewalk_asset_tracker.py~ script automates the creation of the device profile, destination and wireless device certificates in IoT Core.  It outputs the uf2 identity file image that can be used to provision asset tracker devices using the MSD uf2 bootloader.

The script will use the local AWS credentials to create the device profile, destination and wireless device identitiy in IoTCore.

To provision devices, run the following commands in either a Cloud9 environment in your AWS account or on your local machine with established AWS credentials:

```bash
git clone <insert-repo>
cd wm1110-asset-tracker/utils
```
```bash
#install the requirements
python3 -m pip install -r requirements.txt
```

Update the config.yaml if desired to change the desination or specify the device profile to use.  It will create these automatically if they do not already exist.

```bash
python3 provision_sidewalk_asset_tracker.py
```
Download resulting AssetTracker_xxxxx.uf2 file to your local machine, then place the asset tracker device in bootloader mode and copy the file to the bootloader drive to provision the Sidewalk device identity.


- NOTE: Multiple identities can be generated with the '-i' option.  ex. provision 5 devices:
```bash
python3 provision_sidewalk_asset_tracker.py -i 5
```

