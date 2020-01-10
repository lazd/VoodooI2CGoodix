
[![Releases](https://img.shields.io/github/release/lazd/VoodooI2CGoodix.svg)](https://github.com/lazd/VoodooI2CGoodix/releases) 
[![Gitter chat](https://img.shields.io/gitter/room/nwjs/nw.js.svg?colorB=ed1965)](https://gitter.im/lazd/VoodooI2CGoodix) 
[![Donate on patreon](https://img.shields.io/badge/patreon-donate-green.svg)](https://www.patreon.com/lazd)



# VoodooI2CGoodix

VoodooI2CGoodix is a [VoodooI2C satellite](https://github.com/alexandred/VoodooI2C) that enables touchscreen support for the Goodix GT9111 and others in macOS.

## Installation

1. Ensure your [DSDT is fully patched](https://github.com/alexandred/VoodooI2C-Patches), [GPIO pinned](https://voodooi2c.github.io/#GPIO%20Pinning/GPIO%20Pinning), and that VoodooI2C recognizes your I2C controllers. This kext will not work otherwise.

2. Ensure you've already installed the [latest version of VoodooI2C](https://github.com/alexandred/VoodooI2C/releases)

3. Download [the latest VoodooI2CGoodix release](https://github.com/lazd/VoodooI2CGoodix/releases) from the releases page.

4. Copy `VoodooI2CGoodix.kext` to `/Volumes/EFI/EFI/CLOVER/kexts/Other/` and restart.

## Usage

If installation was successful, you should now be able to tap and drag on the touchscreen. In addition, all trackpad gestures are supported, such as two finger scrolling, pinch to zoom, twist to rotate, etc. See the Trackpad preference pane in System Preferences for configuration and examples.

You will want to set your scroll direction to "Natural" in the Trackpad preference pane so scrolling with the touchscreen is intuitive.

You can also right click by tapping and holding.

## Debugging

Ping us [on gitter](https://gitter.im/lazd/VoodooI2CGoodix) and include a `.zip` file with the following:

1. `panic.txt`: If you're experiencing a kernel panic, ensure you have the `keepsyms=1` boot argument in your Clover `config.plist` so that your system will present you with a dialog to report the issue after the panic. Click "Report" and copy the full text of the kernel panic and include it.

2. `VoodooI2CLog.txt`: Logs releated to VoodooI2C. Dump logs with the following command:

```
sudo log show --predicate "processID == 0" --last 10m --debug --info | grep VoodooI2C > ~/Desktop/VoodooI2CLog.txt
```

3. `DSDT.aml`: Include your patched DSDT from `/Volumes/EFI/EFI/CLOVER/ACPI/patched/DSDT.aml`.

4. `IOReg`: Include a copy of the IOReg dumped by [IORegistryExplorer v2.1](https://www.tonymacx86.com/threads/guide-how-to-make-a-copy-of-ioreg.58368/)

5. `info.txt`: Include the following information:

* macOS version and build number from Apple Menu -> About This Mac (i.e. 10.5.2 19C57)
* `VoodooI2C.kext` version number (right click, Get Info)
* `VoodooI2CGoodix.kext` version number (right click, Get Info)

i.e.

```
macOS 10.5.2 19C57
VoodooI2C 2.3
VoodooI2CGoodix 0.2.0
```

## License

This program is protected by the GPL license. Please refer to the [LICENSE](LICENSE) file for more information
