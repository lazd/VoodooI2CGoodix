---
name: "🚫 It doesn't work"
about: VoodooI2CGoodix is not working
title: ''
labels: question
assignees: ''

---

## Prerequisites
<!-- Do not submit an issue unless you can check all of these boxes, or it will be closed immediately -->
<!-- Check boxes by placing an x in them: [x] -->

* [ ] I have followed the [VoodooI2C installation instructions](https://voodooi2c.github.io/#Installation/Installation)
* [ ] I have [patched my DSDT](https://github.com/alexandred/VoodooI2C-Patches)
* [ ] I have followed the [GPIO pinning guide](https://voodooi2c.github.io/#GPIO%20Pinning/GPIO%20Pinning)
* [ ] VoodooI2C recognizes my I2C controllers during a verbose boot
* [ ] I have followed the [VoodooI2CGoodix installation instructions](https://github.com/lazd/VoodooI2CGoodix#installation)
* [ ] I have tried tapping my screen and nothing happens

## Environment
 - **Computer**: <!-- Chuwi Minibook 8 -->
 * **Goodix Touchscreen model**: <!-- GT911 -->
 - **VoodooI2C version:** <!-- 2.2 -->
 - **VoodooI2CGoodix version:** <!-- 0.1.0 -->
 - **macOS Version:** <!-- 10.5.2 19C57 -->

## `DSDT.zip`
<!--
	1. Mount your EFI partition with these instructions https://www.modmy.com/how-mount-your-efi-partition-macos
	2. Find your DSDT.aml at /Volumes/EFI/EFI/CLOVER/ACPI/patched/DSDT.aml
	3. Attach a .zip file containing your DSDT.aml
-->


## `IOReg.zip`
<!--
	1. Dump your IORegistry with these instructions: https://www.tonymacx86.com/threads/guide-how-to-make-a-copy-of-ioreg.58368/
	2. Attach a .zip file containing your IOReg
-->


## `VoodooI2C.log`
<!--
  1. Run the following command in Terminal to dump logs from the last 10 minutes:
	sudo log show --predicate "processID == 0" --last 10m --debug --info | grep VoodooI2C > ~/Desktop/VoodooI2C.log
  2. Attach the log file
-->


## Additional context
<!-- Provide any additional information that might help us debug the issue -->

