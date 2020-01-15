---
name: "💣 Kernel panic"
about: VoodooI2CGoodix is causing a kernel panic
title: ''
labels: bug, kernel panic
assignees: ''

---

## Panic
<!--
	1. Ensure you have the keepsyms=1 boot argument in your Clover config.plist
	2. After restart, the system will present you with a dialog asking you to report the issue
	3. Click "Report" and copy the full text of the kernel panic and paste it below
-->

```
<!-- Paste your panic here -->
```


## Steps to reproduce

1. Touch something...
2. Do something else...
3. Panic!


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

