# candleLight_gsusb

This is firmware for certain STM32F042x/STM32F072xB-based USB-CAN adapters, notably:
- candleLight: https://github.com/HubertD/candleLight (STM32F072xB)
- cantact: http://linklayer.github.io/cantact/ (STM32F042C6)
- canable (cantact clone): http://canable.io/ (STM32F042C6)
- USB2CAN: https://github.com/roboterclubaachen/usb2can (STM32F042x6)
- CANAlyze: https://kkuchera.github.io/canalyze/ (STM32F042C6)
- VulCAN Gen1: https://shop.copperforge.cc/products/ac41 (STM32F042x6)
- Entreé: https://github.com/tuna-f1sh/entree (STM32F042x6)

Of important note is that the common STM32F103 will NOT work with this firmware because its hardware cannot use both USB and CAN simultaneously.
Beware also the smaller packages in the F042 series which map a USB and CAN_TX signal on the same pin and are therefore unusable !

This implements the interface of the mainline linux gs_usb kernel module and
works out-of-the-box with linux distros packaging this module, e.g. Ubuntu.

# Entreé Fork

## USB Power Delivery (USB-PD)

The on-board USB-C controller (STUSB4500) is configured for 5 V / 1A power delivery by default (PDO 2). One can configure the controller using the below CAN bus commands when using the [**candleLight_fw**](https://github.com/tuna-f1sh/candleLight_fw) fork and with the [internal CAN IDs switch](#dip-switches) set.

These commands are scraped from the recieved gs_usb Tx commands and will not be forwarded to the CAN bus when the switch is set. Ensure the DLC is 8 bytes, the ID is correct and byte seven is the Entreé key '0xAF'.

### CAN Bus Side Control

The commands are also scraped from the `can_recieved` callback. For this to work, initiate a connection over USB to setup the desired CAN bit timing then send the 'Save CAN' command (0x06) with the NVM byte set. The CAN bus will now be enabled on power up before USB enumeration. To revert this behaviour, send the same command with the NVM byte unset.

| ID    | Cmd          | 0    | 1          | 2          | 3         | 4         | 5         | 6         | 7    | Action                                                                  |
|-------|--------------|------|------------|------------|-----------|-----------|-----------|-----------|------|-------------------------------------------------------------------------|
| 0x010 | VBUS EN      | 0x01 | NVM (bool) | SET (bool) | 0x00      | 0x00      | 0x00      | 0x00      | 0xAF | Set VBUS always enable (NVM) or try to enable VBUS by setting profile 1 |
| 0x010 | Set PDO      | 0x02 | NVM (bool) | PROFILE    | VOLTAGE_L | VOLTAGE_H | CURRENT_L | CURRENT_H | 0xAF | Set power delivery profile number (1-3) voltage (mV) and current (mA)   |
| 0x010 | Set Profiles | 0x03 | NVM (bool) | PROFILES   | 0x00      | 0x00      | 0x00      | 0x00      | 0xAF | Set number of profile in use (1-3)                                      |
| 0x010 | Set VBUS     | 0x04 | VOLTAGE_L  | VOLTAGE_H  | 0x00      | 0x00      | 0x00      | 0x00      | 0xAF | Request voltage on VBUS (volatile)                                      |
| 0x010 | Get RDO      | 0x05 | 0x00       | 0x00       | 0x00      | 0x00      | 0x00      | 0x00      | 0xAF | Get enumerated profile number                                           |
| 0x010 | Save CAN     | 0x06 | NVM (bool) | 0x00       | 0x00      | 0x00      | 0x00      | 0x00      | 0xAF | Save current CAN bit timing to NVM and auto-enable at start-up for CAN bus side control |

### Usage Notes

* NVM flag will write to the NVM rather than volatile register. If a write is required, the **STUSB4500 will be soft reset** in order to re-enumerate the USB-PD profiles. The LEDs will flash rapidly 20 times when a NVM flash occurs. The volatile settings will also trigger a soft reset in order to re-enumerate. **A soft reset will mean the CAN network will need re-creating on the host**.
* The NVM VBUS enable is the only concrete way to force VBUS; the volatile method attempts to enumerate a 5 V power delivery profile but this may not work with non-compliant devices.
* When profile 2 is enumerated, the orange 'PD-OK' will illuminate. This can be changed to profile 3 with the solder link on the underside of the board.
* Refer to the [STUSB4500 programming guide](https://www.st.com/resource/en/user_manual/dm00664189-the-stusb4500-software-programing-guide-stmicroelectronics.pdf) for more information.
* **Excerise caution** configuring these and use a USB-PD checker/multimeter to verify your configuration prior to powering a device!

### Examples

Using SocketCAN `cansend` command.

```
# enable vbus always (regardless of USB-PD profile enumeration)
cansend can0 010#01010100000000AF
# set PD2 in NVM to 12000 mV / 1000 mA
cansend can0 010#020102E02EE803AF
# save CAN bit timing for auto enable without USB
cansend can0 010#06010000000000AF
# disable auto CAN bus enable
cansend can0 010#06000000000000AF
# read current negotiated profile (returns on same ID B0: CMD, B1: PROFILE)
cansend can0 010#05000000000000AF
```

## Feature Road Map

- ~~Add flash write of bit-timing when set via gs_usb for retrival and CAN enable when no USB connected and Entreé CAN ID config enabled.~~ Added as CMD 0x06
- Add gs_usb support for Entreé config commands and driver patch rather than scrapping the CAN messages.

# Known issues

Be aware that there is a bug in the gs_usb module in linux<4.5 that can crash the kernel on device removal.

Here is a fixed version that should also work for older kernels:
  https://github.com/HubertD/socketcan_gs_usb

The Firmware also implements WCID USB descriptors and thus can be used on recent Windows versions without installing a driver.

# Building

Building requires arm-none-eabi-gcc toolchain.

```shell
sudo apt-get install gcc-arm-none-eabi

mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi-8-2019-q3-update.cmake

# or,
# cmake-gui ..
# don't forget to specify the cmake toolchain file before configuring.
#
# compile all targets :

make

# OR, each board target is a cmake option and can be disabled before running 'make';
# OR, compile a single target , e.g.
make cantact_fw

#
# to list possible targets :
make help

```

## Flashing

Flashing candleLight on linux: (source: [https://wiki.linklayer.com/index.php/CandleLightFirmware](https://wiki.linklayer.com/index.php/CandleLightFirmware))
- Flashing requires the dfu-util tool. On Ubuntu, this can be installed with `sudo apt install dfu-util`.
- compile as above, or download the current binary release: gsusb_cantact_8b2b2b4.bin
- If dfu-util fails due to permission issues on Linux, you may need additional udev rules. Consult your distro's documentation and see `70-candle-usb.rules` provided here.

### recommended simple method
- If compiling with cmake, `make flash-<targetname_fw>`, e.g. `make flash-canable_fw`, to invoke dfu-util.

### method for reflashing a specific device by serial
- when multiple devices are connected, dfu-util may be unable to choose which one to flash.
- Obtain device's serial # by looking at `dfu-util -l`
- adapt the following command accordingly :
 `dfu-util -D CORRECT_FIRMWARE.bin -S "serial_number_here", -a 0 -s 0x08000000:leave`
- note, the `:leave` suffix above may not be supported by older builds of dfu-util and is simply a convenient way to reboot into the normal firmware.

### fail-safe method (or if flashing a blank device)
- Disconnect the USB connector from the CANtact, short the BOOT pins, then reconnect the USB connector. The device should enumerate as "STM32 BOOTLOADER".

- invoke dfu-util manually with: `sudo dfu-util --dfuse-address -d 0483:df11 -c 1 -i 0 -a 0 -s 0x08000000 -D CORRECT_FIRWARE.bin` where CORRECT_FIRWARE is the name of the desired .bin.
- Disconnect the USB connector, un-short the BOOT pins, and reconnect.




## Links to related projects
* [Cangaroo](https://github.com/HubertD/cangaroo) open source can bus analyzer software
* [Candle.NET](https://github.com/elliotwoods/Candle.NET) .NET wrapper for the candle API
