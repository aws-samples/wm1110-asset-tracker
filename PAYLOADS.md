# Sidewalk Asset Tracker Payload Specifications

This document describes the uplink and downlink message payloads used by the asset tracker.  The payload sizes have been restricted to a maximum of 19 bytes which is the maximum payload size available for the CSS/LoRA PHY type on the Amazon Sidewalk network.  

## Uplink Message Types

| TYPE Value | Name | Description |
| :--: | :--: | :-- |
| 0 | CONFIG | Describes the current configuration of the asset tracker |
| 1 | NOLOC | Current ambient data only.  Sent if no location data is available. |
| 2 | WIFI | Current ambient and WiFi location data | 
| 3 | GNSS | Current ambient and partial GNSS location data in a sequence of messages | 

The devices can send two different types of uplink messages: CONFIG and data (NOLOC, WIFI, GNSS).  The config message is sent at device start and on response to a config downlink.  The data messags are sent at regular intervals determined by the configuration or are triggered by the USER button on the WioTracker 1110.

### CONFIG Uplink Message Format
|      |       |       |       |       |       |       |       |       |       |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Name** | Type | Device Time | SW Version | Stored Rec Count | Link / Location Mode | Motion Period | Motion Sampling Freq | Motion Threshold | Static Sampling Freq |
| **Position** | 0x00 | Bytes 2 - 5 | Byte 6 | Bytes 7 - 8 | Byte 9 | Byte 10 | Byte 11 | Byte 12 | Byte 13 |

Description:

| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint_8 | CONFIG message type (0) <br> *ex. 0x00 = CONFIG message* |
| 1-4 | Device Time | uint_32 | Epoche time from device in seconds.<br> *ex. 0x65073638 = 1694971448 = Sun, 17 Sep 2023 17:24:08 GMT* |
| 5 | SW Version | uint_8 | bit 7-4: Major SW Version<br>bit 3-0: Minor SW Version<br> *ex. 0x11 = SW v1.1* |
| 6-7 | Stored Record Count(n) | uint_16 | Current location record count stored in onboard flash.<br> *ex. 0x1234 = 4660 location records stored* |
| 8 | Sidewalk Link Type & Location Scanning Modes | uint_8 | bit 7-4: Sidewalk Link Type<br>-->0b0000 = 0x0 = BLE<br>-->0b0001 = 0x1 = FSK (not used/implemented)<br>-->0b0010 = 0x2 = LoRa<br> bit 3-0: Location Scanning Mode<br>-->0b0000 = 0x0 = WiFi Only<br>-->0b0001 = 0x1 = GNSS Only<br>-->0b0010 = 0x2 = WiFi + GNSS<br>*ex 0x22 = LoRa Link and WiFi + GNSS scanning* |
| 9 | Motion Period(min) | uint_8 | Period of time in minutes the device will stay in the motion state measured from when the motion threshold was last crossed.<br> *ex. 0x05 - 5min* |
|10 | Motion Sampling Frequency (s) | uint_8 | Frequency in seconds for sampling location data and attempting uplinks while device is in the motion state. <br>(Minimum value = 30s) <br> *ex. 0x3C = 60s* |
|11 | Motion Threshold(g) | uint_8 | Acceleration(g) setting for determining whether the tracker is static or in motion. <br> *ex. TBD* |
|12 | Static Sampling Frequecy (min) | uint_8 | Frequency in hours for sampling location data and attempting uplinks while device is in the static state. <br>(Minimum value = 15 - disabled/always in motion)<br> *ex. 0x3C = 60min sample and uplink frequecy* |


### NOLOC Uplink Message Format
|      |       |       |       |       |  
| :--- | :---: | :---: | :---: | :---: | 
| **Name** | Type | Reserved | Temp & Humidity | Motion State & Max Accel |
| **Position** | 0x10 | Byte 2 | Byte 3 | Byte 4 |

Description:
| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint_8 | NOLOC message type<br> bit 7-6: TYPE = 1 (NOLOC)<br>bit 5-0: Unused <br> *ex. 0x40 = NOLOC type* |
| 1 | Reserved | uint_8 | Reserved |
| 2 | Temp & Humidity | uint_8 | bit 7-4: Temperature (C)<br>bit 3-0: Relative Humidity (%) <br> *ex. TBD* |
| 3 | Motion State & Max Accel | uint_8 | bit 7: Motion state<br>bit 6-0: Max accelleration since last record <br> *ex. TBD* |


### WIFI Location Uplink Message Format
|      |       |       |       |       |       | 
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Name** | Type | Reserved | Temp & Humidity | Motion State & Max Accel | WiFi Location Data |
| **Position** | 0x20 | Byte 2 | Byte 3 | Byte 4 | Bytes 5 - 18 |

Description:
| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint_8 | WIFI message type<br> bit 7-6: TYPE = 2 (WIFI)<br>bit 5-3: total fragments<br> bit 2-0: current fragment<br>  *ex. 0x20 = WIFI type * |
| 1 | Reserved | uint_8 | Reserved |
| 2 | Temp & Humidity | uint_8 | bit 7-4: Temperature (C)<br>bit 3-0: Relative Humidity (%) <br> *ex. TBD* |
| 3 | Motion State & Max Accel | uint_8 | bit 7: Motion state<br>bit 6-0: Max accelleration since last record <br> *ex. TBD* |
| 5-19 | WiFi Location Data | | byte 5: RSSI 1<br>byte 6-11: MAC 1<br>byte 12: RSSI 2<br>byte 13-18: MAC 2 |

|      |       |       |  
| :--- | :---: | :---: | 
| **Name** | Type / Seq>0 | GNSS Location Data |
| **Position** | 0x3X | Bytes 2 - 19 |

### GNSS Location Uplink Message Format
|      |       |       |       |       |       |       |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **Name** | Type & Seq=0 | Battery | Temp & Humidity | Motion State & Max Accel | GNSS Data Size | Capture Time |
| **Position** | 0x30 | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Bytes 5-8  |

Description:
| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint_8 | GNSS message type<br> bit 7-6: TYPE = 3 (GNSS)<br>bit 5-0: Sequence number <br> *ex. 0x30 = GNSS seq start * |
| 1 | Reserved | uint_8 | Reserved |
| 2 | Temp & Humidity | uint_8 | bit 7-4: Temperature (C)<br>bit 3-0: Relative Humidity (%) <br> *ex. TBD* |
| 3 | Motion State & Max Accel | uint_8 | bit 7: Motion state<br>bit 6-0: Max accelleration since last record <br> *ex. TBD* |
| 4 | GNSS Data Size | | TBD |
| 5-8 | Capture Time | | TBD |

|      |       |       |  
| :--- | :---: | :---: | 
| **Name** | Type / Seq>0 | GNSS Location Data |
| **Position** | 0x3X | Bytes 2 - 19 |

Description:
| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |


## Downlink Message Types
The asset tracker supports a single downlink message type (Type=0 - CONFIG) that can be used to update the configuration on the device and send lifecycle commands.

| TYPE Value | Name | Description |
| :--: | :--: | :-- |
| 0 | CONFIG | Config and commands for the asset tracker |

### CONFIG Downlink Message Format
|      |       |       |       |       |       |       |       |       |       |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Name** | Type & Command | Device Time | Max Records Stored | Link / Location Mode | Motion Period | Motion Sampling Freq | Motion Threshold | Static Sampling Freq |
| **Position** | Byte 1 | Bytes 2 - 3 | Bytes 4 - 5 | Bytes 7 - 8 | Byte 9 | Byte 10 | Byte 11 | Byte 12 | Byte 13 |

**TODO** Description:

| Byte Offset | Name | Data Type | Description |
| :--: | :--  | :-------: | :---------- |
| 0 | Type | uint_8 | CONFIG message type (0) <br> *ex. 0x00 = CONFIG message* |
| 1-4 | Device Time | uint_32 | Epoche time from device in seconds.<br> *ex. 0x65073638 = 1694971448 = Sun, 17 Sep 2023 17:24:08 GMT* |
| 5 | SW Version | uint_8 | bit 7-4: Major SW Version<br>bit 3-0: Minor SW Version<br> *ex. 0x11 = SW v1.1* |
| 6-7 | Stored Record Count(n) | uint_16 | Current location record count stored in onboard flash.<br> *ex. 0x1234 = 4660 location records stored* |
| 8 | Sidewalk Link Type & Location Scanning Modes | uint_8 | bit 7-4: Sidewalk Link Type<br>-->0b0000 = 0x0 = BLE<br>-->0b0001 = 0x1 = FSK (not used/implemented)<br>-->0b0010 = 0x2 = LoRa<br> bit 3-0: Location Scanning Mode<br>-->0b0000 = 0x0 = WiFi Only<br>-->0b0001 = 0x1 = GNSS Only<br>-->0b0010 = 0x2 = WiFi + GNSS<br>*ex 0x22 = LoRa Link and WiFi + GNSS scanning* |
| 9 | Motion Period(min) | uint_8 | Period of time in minutes the device will stay in the motion state measured from when the motion threshold was last crossed.<br> *ex. 0x05 - 5min* |
|10 | Motion Sampling Frequency (s) | uint_8 | Frequency in seconds for sampling location data and attempting uplinks while device is in the motion state. <br>(Minimum value = 30s) <br> *ex. 0x3C = 60s* |
|11 | Motion Threshold(g) | uint_8 | Acceleration(g) setting for determining whether the tracker is static or in motion. <br> *ex. TBD* |
|12 | Static Sampling Frequecy (hr) | uint_8 | Frequency in hours for sampling location data and attempting uplinks while device is in the static state. <br>(Minimum value = 0 - disabled/always in motion)<br> *ex. 0x01 = 1hr sample and uplink frequecy* |

