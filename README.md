minipro
========
An opensource rewrittement of autoelectric.cn programming utility

## Features
* Compatibility with Minipro TL866CS and Minipro TL866A
* More than 13000 target devices (including AVRs, PICs, various BIOSes and EEPROMs)
* ZIF40 socket and ISP support
* Vendor-specific MCU configuration bits
* Chip ID verification
* Overcurrency protection
* System testing

## Synopsis

```nohighlight
$ minipro -p ATMEGA48 -w atmega48.bin
$ minipro -p ATMEGA48 -r atmega48.bin
```

## Installing under Debian / Ubuntu

Building a Debian package directly from this repository is as easy as

```nohighlight
sudo apt-get install build-essential git fakeroot dpkg-dev libusb-1.0-0-dev
git clone https://github.com/vdudouyt/minipro/ && cd minipro/
fakeroot dpkg-buildpackage -b
sudo dpkg -i ../minipro_0.1-1_*.deb
```
## Installing under CentOS7

```nohighlight
sudo yum install libusbx libusbx-dev git
git clone https://github.com/vdudouyt/minipro/ && cd minipro/
patch -p1 < CentOS/CentOS7-x86_64.patch
make
sudo make install
sudo udevadm control --reload-rules
```
