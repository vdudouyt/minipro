minipro
========
An open source program for controlling the MiniPRO TL866xx series of chip programmers 

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

## Prerequisites

You'll need some sort of Linux machine.  Other Unices may work, though 
that is untested.  You will need libusb to access the programmer through 
a USB port.  For Debian, Ubuntu, and related distributions, the package 
you want is "libusb-1.0-0-dev".

## Compilation and Installation
```nohighlight
sudo apt-get install build-essential git fakeroot dpkg-dev libusb-1.0-0-dev
git clone https://github.com/vdudouyt/minipro.git
cd minipro
make
sudo make install
``

## Making a .deb file for Debian / Ubuntu

Building a Debian package directly from this repository is easy

```nohighlight
sudo apt-get install build-essential git fakeroot dpkg-dev libusb-1.0-0-dev
git clone https://github.com/vdudouyt/minipro.git
cd minipro
fakeroot dpkg-buildpackage -b
sudo dpkg -i ../minipro_0.1-1_*.deb
```
