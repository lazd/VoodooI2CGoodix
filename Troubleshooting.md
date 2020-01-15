## Troubleshooting VoodooI2CGoodix

VoodooI2CGoodix won't work unless you've followed [these steps](README.md#installation). If you've done each of these steps correctly and it still doesn't work, you can [file an issue](https://github.com/lazd/VoodooI2CGoodix/issues/new/choose) or chat with us [on gitter](https://gitter.im/lazd/VoodooI2CGoodix), providing all of the information below.

### Asking for help on gitter

If ask for help, you must provide the following information at a minimum.

#### Description of the problem

Describe the problem, the steps you took to reproduce it, and what you expected to happen.

#### Screenshot or video

If it will help make it more clear, provide a screenshot or a video illustrating the problem.

#### Environment information

Provide all of the following information
 * **Computer**: i.e. Chuwi Minibook 8
 * **Goodix Touchscreen model**: i.e. GT911
 * **VoodooI2C version:** i.e. 2.3
 * **VoodooI2CGoodix version:** i.e. 0.2.3
 * **macOS Version:** i.e. 10.5.2 19C57

#### `DSDT.aml.zip`

1. Mount your EFI partition with [these instructions](https://www.modmy.com/how-mount-your-efi-partition-macos)
2. Find your `DSDT.aml` at `/Volumes/EFI/EFI/CLOVER/ACPI/patched/DSDT.aml`
3. Attach a .zip file containing your DSDT.aml

#### `IOReg.zip`

1. Dump your IORegistry with [these instructions](https://www.tonymacx86.com/threads/guide-how-to-make-a-copy-of-ioreg.58368/)
2. Attach a .zip file containing your IOReg


#### `VoodooI2C.log`

1. Run the following command in Terminal to dump logs from the last 10 minutes:
```
sudo log show --predicate "processID == 0" --last 10m --debug --info | grep VoodooI2C > ~/Desktop/VoodooI2C.log
```
2. Attach the log file `~/Desktop/VoodooI2C.log`
