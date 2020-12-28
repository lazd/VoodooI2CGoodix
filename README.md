# ⚠️⚠️⚠️ WARNNING: THE CODE IN THIS REPOSITY IS FOR GPD WINMAX ONLY !!! ⚠️⚠️⚠️
Check out MacKernelSDK before you build the source !!

Modifications compared to the original one:
1. Fixed multitouch problem for winmax
2. Fixed initialization problem for winmax, which will fail to load driver occasionally at boot
3. One finger default to scroll, one finger long press act as the original drag
4. Fixed double tap problem

git clone https://github.com/acidanthera/MacKernelSDK

[![Releases](https://img.shields.io/github/release/lazd/VoodooI2CGoodix.svg)](https://github.com/lazd/VoodooI2CGoodix/releases) 
[![Gitter chat](https://img.shields.io/gitter/room/nwjs/nw.js.svg?colorB=ed1965)](https://gitter.im/lazd/VoodooI2CGoodix) 
[![Donate on Patreon](https://img.shields.io/badge/patreon-donate-green.svg)](https://www.patreon.com/lazd)



# VoodooI2CGoodix

VoodooI2CGoodix is a [VoodooI2C satellite](https://github.com/alexandred/VoodooI2C) that enables touchscreen support for the Goodix GT9111 and others in macOS.

## Supported Systems

VoodooI2CGoodix is confirmed working on the following systems:

* Chuwi MiniBook 8
* GPD P2 Max

## Installation

1. Ensure your [DSDT is fully patched](https://github.com/alexandred/VoodooI2C-Patches), [GPIO pinned](https://voodooi2c.github.io/#GPIO%20Pinning/GPIO%20Pinning), and that VoodooI2C recognizes your I2C controllers. This kext will not work otherwise.

2. Ensure you've already installed the [latest version of VoodooI2C](https://github.com/alexandred/VoodooI2C/releases)

3. Download [the latest VoodooI2CGoodix release](https://github.com/lazd/VoodooI2CGoodix/releases) from the releases page.

4. Copy `VoodooI2CGoodix.kext` to `/Volumes/EFI/EFI/CLOVER/kexts/Other/` and restart.

## Usage

If installation was successful, you should now be able to tap and drag on the touchscreen. In addition, all trackpad gestures are supported, such as two finger scrolling, pinch to zoom, twist to rotate, etc. See the Trackpad preference pane in System Preferences for configuration and examples.

Finally, stylus support is present, with right click support using the stylus' button.

You will want to set your scroll direction to "Natural" in the Trackpad preference pane so scrolling with the touchscreen is intuitive.

You can also right click by tapping and holding.

## Support

If you're having problems with VoodooI2CGoodix, you've found a bug, or you have a great idea for a new feature, [file an issue](https://github.com/lazd/VoodooI2CGoodix/issues/new/choose)!

You can also chat with us [on gitter](https://gitter.im/lazd/VoodooI2CGoodix), but provide [all the necessary information](Troubleshooting.md) with your request or you will not be helped.

## Sponsor

If you like VoodooI2CGoodix, you can donate to support its continued development by donating on [Patreon](https://www.patreon.com/lazd)!

## License

This program is protected by the GPL license. Please refer to the [LICENSE](LICENSE) file for more information
