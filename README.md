This project has moved to https://gitlab.com/DavidGriffith/minipro/

# minipro
An open source program for controlling the MiniPRO TL866xx series of chip programmers

## Features
* Compatibility with Minipro TL866CS and Minipro TL866A from
Autoelectric (http://www.autoelectric.cn/en/tl866_main.html)
* More than 13000 target devices (including AVRs, PICs, various BIOSes
and EEPROMs)
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

You'll need some sort of Linux or MacOS machine.  Other Unices may work, 
though that is untested.  You will need version 1.0.16 or greater of libusb.


## Installation on Linux

### Install build dependencies 

#### Debian/Ubuntu
```nohighlight
sudo apt-get install build-essential pkg-config git libusb-1.0-0-dev
```

#### CentOS 7
```nohighlight
sudo yum install gcc make pkgconfig git libusbx-devel
```

### Checkout source code and compile 
```nohighlight
git clone https://github.com/vdudouyt/minipro.git
cd minipro
make
sudo make install
```

### Udev configuration (recommended)
If you want to access the programmer as a regular user, you'll have to
configure udev to recognize the programmer and set appropriate access
permissions.

#### Debian/Ubuntu
```nohighlight
sudo cp udev/debian/60-minipro.rules /etc/udev/rules.d/
sudo udevadm trigger
```
You'll also have to add your regular user to the `plugdev` system
group:
```nohighlight
sudo usermod -a -G plugdev YOUR-USER
```
Note that this change will only become effective after your next
login.

#### CentOS 7
```nohighlight
sudo cp udev/centos7/80-minipro.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```
The CentOS rules file currently make the programmer device writable for 
all users.

### Bash completion (optional)

Bash users may want to optionally install the provided completion file:
```nohighlight
sudo cp bash_completion.d/minipro /etc/bash_completion.d/
```

### Making a .deb package

Building a Debian package directly from this repository is easy.  Make
sure you have the build dependencies installed as described above.  Be
sure it all builds, then do this:

```nohighlight
sudo apt-get install fakeroot debhelper dpkg-dev
fakeroot dpkg-buildpackage -b -us -uc
```

You should then have a .deb package for you to install with `dpkg -i`. 
Note that the .deb package will already provide the udev and 
bash-completion configurations for you.

### Making a .rpm package

You can build RPM packages for Fedora and CentOS with the supplied
minipro.spec.

First make sure you have a RPM build environment set up. You need to have
the rpmdevtools package installed and a `rpmbuild` directory tree within
your homedir. Use the `rpmdev-setuptree` command to create the rpmbuild
directory tree if it does not exist yet.

Since minipro does not yet make official releases with version numbers
and tags, you have to choose a specific git commit to build. Open the
minipro.spec file and adapt the "commit" and "commitdate" definitions.
You can get these either with `git log` or from the github project page.

Then use these commands to download the source tarballs from github and
build the package:

```nohighlight
spectool -g -R minipro.spec
rpmbuild -ba minipro.spec
```

The final RPMs can be found below `~/rpmbuild/RPMS/`

## Installation on macOS

### Install dependencies
Install `pkg-config` and `libusb` using brew or MacPorts:
```
brew install pkg-config
brew install libusb
brew link libusb
```
or:
```
port install pkgconfig
port install libusb
```
### Checkout source code and compile
```nohighlight
git clone https://github.com/vdudouyt/minipro.git
cd minipro
make
sudo make install
```
