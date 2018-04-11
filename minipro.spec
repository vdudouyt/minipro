# .SPEC-file to package RPMs for Fedora and CentOS

# adapt commit id and commitdate to match the git version you want to build
%global commit d3738cd963d1d59baf7207c2aea8746c778b0acf
%global commitdate 20180330

# then download and build like this:
# spectool -g -R SPECS/minipro.spec
# rpmbuild -ba SPECS/minipro.spec

Summary: Program for controlling the MiniPRO TL866xx series of chip programmers
Name: minipro
Version: 0.1
%global shortcommit %(c=%{commit}; echo ${c:0:7})
Release: %{commitdate}.%{shortcommit}%{?dist}
License: GPLv3
URL: https://github.com/vdudouyt/minipro
Source: https://github.com/vdudouyt/%{name}/archive/%{commit}/%{name}-%{shortcommit}.tar.gz
BuildRequires: libusbx-devel

%description
Software for Minipro TL866XX series of programmers from autoelectric.cn
Used to program flash, EEPROM, etc.

%prep
%autosetup -n %{name}-%{commit}

%build
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

install -D -p -m 0644 udev/centos7/80-minipro.rules %{buildroot}/%{_udevrulesdir}/80-minipro.rules
install -D -p -m 0644 bash_completion.d/minipro %{buildroot}/%{_sysconfdir}/bash_completion.d/minipro

%files
%{!?_licensedir:%global license %%doc}
%license LICENSE
%doc README.md
%{_bindir}/minipro
%{_bindir}/minipro-query-db
%{_bindir}/miniprohex
%{_mandir}/man1/%{name}.*
%{_udevrulesdir}/80-minipro.rules
%{_sysconfdir}/bash_completion.d/*
