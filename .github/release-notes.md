# What's new #
Write what's changed here before publishing this draft. Say whether installing only the firmware is enough, and whether going back to an earlier version keeps the clock's settings.

# Installation #
This firmware can't update itself over Wi-Fi, so flash it over USB. The easiest way is the [web installer]($installer$), which works in Chrome or Edge on a computer. The [installation guide](https://github.com/judge2005/EleksTubeIPS/wiki/Installation) covers connecting the clock and backing up its original firmware.

**Upgrading from 1.9.4 or later:** choose **Upgrade** in the web installer, or flash only the firmware file for your clock at `0x10000`. Your settings, Wi-Fi details and uploaded images are kept. In Home Assistant, the existing entities keep their IDs.

```
esptool.py --chip esp32 --port <port> write_flash 0x10000 firmware-elekstubev1-$version$.bin
```

**New install, or upgrading from before 1.9.4:** choose **Full install** in the web installer, or flash `bootloader.bin` at `0x1000`, `partitions.bin` at `0x8000`, `boot_app0.bin` at `0xe000`, the firmware at `0x10000` and `littlefs-$version$.bin` at `0x180000`. A full install replaces your uploaded images with the ones that come with the firmware.

NOTE: littlefs8MB-$version$.bin, bootloader8MB.bin and partitions8MB.bin are only for the IPSTube clocks. For all other clocks use littlefs-$version$.bin, bootloader.bin and partitions.bin.

| Clock | Firmware file |
|---|---|
| EleksTube IPS V1, Xunfeng IPS | `firmware-elekstubev1-$version$.bin` |
| EleksTube IPS V2 | `firmware-elekstubev2-$version$.bin` |
| NovelLife SE | `firmware-novellifese-$version$.bin` |
| Si Hai | `firmware-sihai-$version$.bin` |
| PCBWay PunkCyber | `firmware-punkcyber-$version$.bin` |
| IPSTube (H401/H402) | `firmware-ipstube-$version$.bin` |
| IPSTube with hardware dimming | `firmware-ipstube_dim-$version$.bin` |

Some clocks sold as an EleksTube IPS V2 need the V1 firmware. If every display shows the same digit after installing, install the other EleksTube version's firmware file.

`SHA256SUMS` has a checksum for each file. `firmware-symbols-$version$.zip` is only for decoding a crash's backtrace.
