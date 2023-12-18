# AWS IoT Asset Tracker Device Application for Amazon Sidewalk 

This repo provides a demonstration asset tracker application using Amazon Sidewalk and AWS IoT Core for the [WioTracker 1110 Dev board](https://www.seeedstudio.com/Wio-Tracker-1110-Dev-Board-p-5799.html) from Seeed Studio.  It designed to work with the [AWS IoT Asset Tracker Demo cloud application](https://github.com/aws-samples/aws-iot-asset-tracker-demo).

For steps to build and deploy the cloud application, please follow this guided workshop: [https://catalog.workshops.aws/sidewalk-asset-tracking/en-US](https://catalog.workshops.aws/sidewalk-asset-tracking/en-US)

If you are interested in building the device application, please continue reading.

## Project Overview

This application gathers onboard sensor data as well as WIFI and GNSS signal data and sends it to AWS IoT Core using Amazon Sidewalk.  The WIFI and GNSS data is then resolved to geo coordinates using IoT Core Device Location to enable tracking of assets.

The asset tracker device project consists of three main components:
- the Zephyr application based on the Nordic nRF Connect SDK
- Adafruit UF2 USB bootloader - [bootloader/](https://github.com/aws-samples/wm1110-asset-tracker/tree/main/bootloader)
- Sidewalk device provisioning utility - [utils/](https://github.com/aws-samples/wm1110-asset-tracker/main/dev/utils)

The application and Sidewalk device identity are both intended to be programmed as a UF2 image using the bootloader.  That said, the steps for preparing a device without the bootloader are as follows:

1) Build and program the bootloader using the steps found in the [bootloader dir](https://github.com/aws-samples/wm1110-asset-tracker/tree/main/bootloader).
2) Setup the nRF Connect environment and build the application to produce a UF2 image. (See below)
3) Follow the provisioning tool [instructions](./utils/README.md) to create a Sidewalk identity UF2 image. 
4) Program the device application and identity UF2 images using the UF2 bootloader.

Detailed steps for programming and provisioning the device with UF2 images can be found in the [workshop content](https://catalog.workshops.aws/sidewalk-asset-tracking/en-US/002-device-setup/001-programming).


## Building the application

<details>
    <summary><b>Building the application on a local development environment (MacOS) </b></summary>

   - STEP 1: install the nRF Connect SDK
       - [automatically](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/assistant.html) (recommended)
       - [manually](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/installing.html)

   - STEP 2: Open terminal for the v2.5.0 SDK from the nRF Connect Toolchain Manager

   - STEP 3: In the terminal, enable the Sidewalk SDK and install the requirements from the root of the Nordic SDK (ex. '/opt/nordic/ncs/v2.5.0/')
       ```bash
       west config manifest.group-filter "+sidewalk"
       west update
       ```
       ```bash
       pip install -r sidewalk/requirements.txt
       ```

   - STEP 4: Clone the asset tracker application firmware to the samples dir in the sidewalk sdk
       ```bash
       git clone --recurse-submodules https://github.com/aws-samples/wm1110-asset-tracker sidewalk/samples/wm1110-asset-tracker
       ```

   - STEP 5: Apply the Semtech LR1110 patch and copy in the libraries from the root of the sidewalk sdk dir
       ```bash
       cd sidewalk/
       patch -p1 < samples/wm1110-asset-tracker/SWDR006/nRF52840_LR11xx_driver_v010000.diff
       cp samples/wm1110-asset-tracker/SWDR006/lib*.a lib/lora_fsk/
       ```  

   - STEP 6: Build the WM1110 application firmware
       ```bash
       cd samples/wm1110-asset-tracker/
       west build -b wio_tracker_1110 -- -DRADIO=LR1110 -DBOARD_ROOT=.
       ```
    
   The UF2 image for the application will be found here:  `build/zephyr/AssetTrackerDeviceApp.uf2`
</details>

<details>
    <summary><b>Building the application in a AWS Cloud9 environment </b></summary>

- From an appropriately sized C9/EC2 instance (ex c5.xlarge) using Ubuntu 22.04, execute the following to install - 
    - nRF Connect SDK v2.5.0 (in ~/ncs)
    - Zephyr (in ~/ncs/zephyr)
    - west (in ~/.local/bin)
    - Zephyr SDK v0.16.4 (in ~/zephyr-sdk-0.16.4)
    - Sidewalk SDK v1.15.0 (in ~/ncs/sidewalk)

**NOTE:** The build environment requires at least 20GB of storage.  For a newly created C9 env (10GB default), you will need to resize the EBS volume to 20GB or larger by following [these instructions](https://docs.aws.amazon.com/cloud9/latest/user-guide/move-environment.html#move-environment-resize). 

```bash
cd ~
# install nRF Connect SDK
wget https://apt.kitware.com/kitware-archive.sh
sudo bash kitware-archive.sh
rm kitware-archive.sh
sudo apt install --no-install-recommends git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

```bash
# install west
pip3 install --user west
echo 'export PATH=~/.local/bin:"$PATH"' >> ~/.bashrc
source ~/.bashrc
```

```bash
# get the nRF Connect SDK
mkdir -p ~/ncs
cd ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.5.0
west update
west zephyr-export
```

```bash
# additional dependencies
pip3 install --user -r zephyr/scripts/requirements.txt
pip3 install --user -r nrf/scripts/requirements.txt
pip3 install --user -r bootloader/mcuboot/scripts/requirements.txt
```

```bash
# install zephyr SDK
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.4/zephyr-sdk-0.16.4_linux-x86_64.tar.xz
wget -O - https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.4/sha256.sum | shasum --check --ignore-missing
tar xvf zephyr-sdk-0.16.4_linux-x86_64.tar.xz
rm zephyr-sdk-0.16.4_linux-x86_64.tar.xz
cd ~/zephyr-sdk-0.16.4
./setup.sh
```

```bash
# enable the Sidewalk SDK and install the requirements
cd ~/ncs
west config manifest.group-filter "+sidewalk"
west update

pip install -r sidewalk/requirements.txt
```

```bash
# clone the wm1110-asset-tracker repo into the sidewalk samples dir
cd ~/ncs
git clone --recurse-submodules https://github.com/aws-samples/wm1110-asset-tracker sidewalk/samples/wm1110-asset-tracker
```

```bash
# apply the LR1110 patch to the sidewalk SDK and copy in the LR11xx libraries
cd ~/ncs/sidewalk
patch -p1 < samples/wm1110-asset-tracker/SWDR006/nRF52840_LR11xx_driver_v010000.diff
cp samples/wm1110-asset-tracker/SWDR006/lib*.a lib/lora_fsk/
```

```bash
# build it
cd ~/ncs/sidewalk/samples/wm1110-asset-tracker
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_BASE=~/ncs/zephyr
export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-0.16.4
west build -b wio_tracker_1110 -- -DRADIO=LR1110 -DBOARD_ROOT=.
```

The UF2 image for the application will be found here:  `~/ncs/sidewalk/samples/wm1110-asset-tracker/build/zephyr/AssetTrackerDeviceApp.uf2`
</details>

## Security

The sample code; software libraries; command line tools; proofs of concept; templates; or other related technology (including any of the foregoing that are provided by our personnel) is provided to you as AWS Content under the AWS Customer Agreement, or the relevant written agreement between you and AWS (whichever applies). You should not use this AWS Content in your production accounts, or on production or other critical data. You are responsible for testing, securing, and optimizing the AWS Content, such as sample code, as appropriate for production grade use based on your specific quality control practices and standards. Deploying AWS Content may incur AWS charges for creating or using AWS chargeable resources, such as running Amazon EC2 instances or using Amazon S3 storage.

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This library is licensed under the MIT-0 License. See the [LICENSE](LICENSE) file.







