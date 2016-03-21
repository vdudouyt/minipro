minipro
========
An open source program for controlling the MiniPRO TL866xx series of chip programmers 

## Features
* Compatibility with Minipro TL866CS and Minipro TL866A from 
Autoelectric (http://www.autoelectric.cn/)
* More than 13000 target devices (including AVRs, PICs, various BIOSes and EEPROMs)
* ZIF40 socket and ISP support
* Vendor-specific MCU configuration bits
* Chip ID verification
* Overcurrent protection
* System testing

## Synopsis

```nohighlight
$ minipro -p ATMEGA48 -w atmega48.bin
$ minipro -p ATMEGA48 -r atmega48.bin
```

## Prerequisites

You'll need some sort of Linux machine.  Other Unices may work, though 
that is untested.  You will need version 1.0.16 or greater of libusb. 
Debian 7 (Wheezy) users should do this to make sure you get the right 
version:

```sudo apt-get -t wheezy-backports libusb-1.0-0-dev```

## Compilation and Installation
```nohighlight
sudo apt-get install build-essential git fakeroot dpkg-dev libusb-1.0-0-dev
git clone https://github.com/vdudouyt/minipro.git
cd minipro
make
sudo make install
```

## Making a .deb file for Debian / Ubuntu

Building a Debian package directly from this repository is easy.  Make 
sure you have the packages described above installed.  Be sure it all 
builds, then do this:

```nohighlight
sudo apt-get install fakeroot dpkg-dev
fakeroot dpkg-buildpackage -b
```

You should then have a .deb file for you to install with ```dpkg -i```.
