# BMP280 I2C driver on Raspberry Pi 4B (custom Buildroot image)

A learning project: a minimal Buildroot Linux image for the Raspberry Pi 4B, plus a Linux I2C driver for the BMP280 temperature/pressure sensor, written step by step.

Series on dev.to: _(add link)_

> **Status:** Part 1, skeleton driver (registers with the I2C core, `probe()` and `remove()` print to `dmesg`). Chip ID, Device Tree and real readings come in later parts.

## Tested with

| Item | Version |
|---|---|
| Board | Raspberry Pi 4B (4 GB) |
| Kernel | 6.12.x (Raspberry Pi kernel, built by Buildroot) |
| Buildroot | 2026.02.1 |
| Host | Ubuntu / any Linux |

## Repo layout

```
.
├── Bmp280_driver/          # out-of-tree kernel module (C source + Makefile)
├── config/
│   ├── rpi4b_defconfig     # Buildroot config
│   └── linux.config        # kernel config
├── rasberrypi_overlay/     # rootfs overlay (files copied into the image)
│   └── etc/
│       ├── network/interfaces
│       └── ssh/sshd_config
├── examples/
│   └── wpa_supplicant.conf.example   # copy and edit (see step 4)
└── README.md
```

`rasberrypi_overlay/etc/wpa_supplicant.conf` holds real Wi-Fi credentials, so it is **git-ignored**. You create it yourself from the example.

## Hardware

**BMP280 to Pi header** (physical pin numbers):

| BMP280 | Pi pin |
|---|---|
| VCC | 1 (3.3V) |
| GND | 6 (GND) |
| SDA | 3 (GPIO2) |
| SCL | 5 (GPIO3) |

CSB and SDO: see the datasheet, the I2C address is `0x76` when SDO is tied to GND and `0x77` when tied to VCC.

**Optional USB-serial adapter (CP2102), for the boot console:**

| Adapter | Pi pin |
|---|---|
| GND | 6 |
| RXD | 8 (Pi TX) |
| TXD | 10 (Pi RX) |

Cross TX and RX. Do **not** connect the adapter's 5V or 3V3 pins; the Pi is powered separately.

## 1. Host prerequisites

Buildroot needs the usual build tools. On Debian/Ubuntu:

```bash
sudo apt install build-essential git wget cpio unzip rsync bc file libncurses-dev \
    python3 libssl-dev
```

See the Buildroot manual for the full list for your Buildroot version.

## 2. Get the sources

```bash
git clone <this-repo-url> my_rasberrypi_project
git clone https://gitlab.com/buildroot.org/buildroot.git
```

If you want the exact Buildroot revision used here, check it out at the commit listed in the table above.

## 3. Load the Buildroot config

```bash
cd buildroot
cp ../my_rasberrypi_project/config/rpi4b_defconfig configs/
make rpi4b_defconfig
```

### Fix the paths for your machine

The defconfig contains **absolute paths from the author's machine**. Change them to where you cloned this repo. Either use `make menuconfig`:

- System configuration → *Root filesystem overlay directories* → `/path/to/my_rasberrypi_project/rasberrypi_overlay`
- Kernel → *Configuration file path* → `/path/to/my_rasberrypi_project/config/linux.config`

or edit these lines in `.config`/the defconfig before building:

```
BR2_ROOTFS_OVERLAY="/path/to/my_rasberrypi_project/rasberrypi_overlay"
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/path/to/my_rasberrypi_project/config/linux.config"
```

Check that `BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG=y` is set.

## 4. Wi-Fi credentials (not stored in the repo)

```bash
cd my_rasberrypi_project
cp examples/wpa_supplicant.conf.example rasberrypi_overlay/etc/wpa_supplicant.conf
# edit it: set your SSID and password
```

Skip this if you plan to use Ethernet only.

## 5. Login credentials (development only)

The image is configured with:

- user: `root`
- password: `123`
- SSH: root login with password is **enabled** in `sshd_config`

This is a deliberate shortcut for a dev board on a home network. Change the password (`menuconfig` → System configuration → Root password) and do not expose the board to the internet.

## 6. Build

```bash
cd buildroot
make -j"$(nproc)"
```

The first build takes a long time. The image ends up at `output/images/sdcard.img`.

## 7. Write the image

Find your target device first, and check twice:

```bash
lsblk        # run before and after plugging the drive in; the new entry is yours
sudo umount /dev/sdX* 2>/dev/null
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M status=progress conv=fsync
```

Replace `/dev/sdX` with the whole disk (not a partition). Writing to the wrong device destroys its data.

## 8. Boot configuration

Mount the boot (FAT) partition and check `config.txt` and `cmdline.txt`.

`config.txt` should contain:

```
enable_uart=1
dtparam=i2c_arm=on
```

`cmdline.txt`: the `root=` value must match where you boot from.

- SD card: `root=/dev/mmcblk0p2` (or the PARTUUID form)
- USB drive: `root=/dev/sda2`

If the kernel log stops at `Waiting for root device`, this is the cause: the root device in `cmdline.txt` does not match the device the system is on.

## 9. First boot and access

Serial console (optional):

```bash
sudo picocom -b 115200 /dev/ttyUSB0
```

Garbled output usually means wrong wiring (TX/RX) or wrong baud rate.

SSH (once the board has an IP):

```bash
ssh root@<board-ip>
```

Check the I2C bus:

```bash
ls /dev/i2c*
i2cdetect -y 1        # the BMP280 should appear at 0x76 (or 0x77)
```

If `/dev/i2c-1` is missing, load the drivers by hand:

```bash
modprobe i2c-bcm2835
modprobe i2c-dev
```

and confirm `CONFIG_I2C_BCM2835` and `CONFIG_I2C_CHARDEV` are enabled in the kernel config.

Keep the in-tree `CONFIG_BMP280` disabled so it does not compete with this driver for the device.

## 10. Build and load the driver

Build the module against the kernel that Buildroot built (the build tree is in `output/build/linux-<version>`):

```bash
cd my_rasberrypi_project/Bmp280_driver
make -C /path/to/buildroot/output/build/linux-<version> \
     ARCH=arm64 \
     CROSS_COMPILE=/path/to/buildroot/output/host/bin/aarch64-linux- \
     M=$(pwd) modules
```

(Check the exact prefix with `ls /path/to/buildroot/output/host/bin | grep gcc`. If the Makefile in this folder already wraps this, use it as written.)

Copy it to the board and test:

```bash
scp bmp280.ko root@<board-ip>:/root/

# on the board
insmod bmp280.ko                          # registers the driver; nothing prints yet
echo my-bmp280 0x76 > /sys/bus/i2c/devices/i2c-1/new_device
dmesg | tail                              # probe() message appears here
echo 0x76 > /sys/bus/i2c/devices/i2c-1/delete_device   # remove() runs
rmmod bmp280
```

Use the same name you put in the driver's `i2c_device_id` table (shown here as `my-bmp280`).

## Troubleshooting

| Symptom | Check |
|---|---|
| Module loads but nothing prints | Is `module_i2c_driver(...)` in the source? Did you bind a device with `new_device`? Does the name match the ID table exactly? |
| `i2cdetect` shows an empty grid | Wiring (SDA/SCL), solder joints, 3.3V |
| Boot log stops at `Waiting for root device` | `root=` in `cmdline.txt` |
| Garbled serial output | TX/RX crossed? baud 115200? |
| No `/dev/i2c-1` | `dtparam=i2c_arm=on`, I2C kernel options, `modprobe` |

## Notes

- This is a learning project, not production code.
- Do not commit `rasberrypi_overlay/etc/wpa_supplicant.conf` or any file with real credentials.