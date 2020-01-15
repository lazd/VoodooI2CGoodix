---
name: "🐛 Bug report"
about: VoodooI2CGoodix is working, but there's a bug with its functionality
title: ''
labels: bug
assignees: ''

---

## Description
<!-- Describe the bug -->


## Steps to reproduce

1. Touch something...
2. Do something else...
3. Then this happened...


## Expected behavior
<!-- Describe what you expected to happen -->


## Screenshot or video
<!-- If applicable, add screenshots or videos to help explain the problem -->


## Environment
 - **Computer**: <!-- Chuwi Minibook 8 -->
 * **Goodix Touchscreen model**: <!-- GT911 -->
 - **VoodooI2C version:** <!-- 2.2 -->
 - **VoodooI2CGoodix version:** <!-- 0.1.0 -->
 - **macOS Version:** <!-- 10.5.2 19C57 -->

## `DSDT.aml.zip`
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

