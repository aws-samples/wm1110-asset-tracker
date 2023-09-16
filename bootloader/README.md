
## Building the bootloader for the WioTracker1110:
1. Clone the AdaFruit bootloader and check out the v0.7.0 release tag:
```
git clone https://github.com/adafruit/Adafruit_nRF52_Bootloader
cd Adafruit_nRF52_Bootloader/
git checkout tags/0.7.0 -b v0.7.0
```

2. Apply the patch to add the WioTracker1110 board target:
```
git apply ../0001-add-WioTracker1110-board-target.patch
```

3. Deploy submodules and build:
*NOTE: If you have not already, be sure to install the [prerequisites](https://github.com/adafruit/Adafruit_nRF52_Bootloader/blob/master/README.md#prerequisites) to setup the env to build the bootloader.*
```
git submodule update --init
make BOARD=wio_tracker_1110 all
```

4. With the SWD Tag-Connect cable and JLink probe attached, flash the board:
```
make BOARD=wio_tracker_1110 flash
```

Alternatively, you can flash the prebuilt bootloader image (*ex. wio_tracker_1110_bootloader-0.7.0_nosd.hex*) included in this repo with the nRF Programmer utility included with [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop) or with nRF prog CMD line tools.


## Debug
The bootloader can be built to provide some limited debug output over RTT, but these [additional commits](https://github.com/adafruit/Adafruit_nRF52_Bootloader/pull/280) are needed for it to build.  Without the commits and included debug linker script, the bootloader fails to build/link due to debug image size.  Either pick the commits or just clone main if you want to debug.
```
make BOARD=wio_tracker_1110 clean
make BOARD=wio_tracker_1110 DEBUG=1 all
make BOARD=wio_tracker_1110 DEBUG=1 flash
```
The output will show the addresses being written when a uf2 file is copied to the boot drive and blocks written to the SoC flash.  This is helpful for debugging UF2 file creation to ensure that the appropriate offsets are being used.
