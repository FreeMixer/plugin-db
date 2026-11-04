Name: plugin-db
Version: 0.1.0
Release: 1%{?dist}
License: GPL-3.0-or-later
Summary: The pin of a plugin: its key, its words and its rules
URL: https://github.com/FreeMixer/plugin-db

Source0: %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildArch: noarch

BuildRequires: gcc
BuildRequires: make
BuildRequires: pkgconfig
BuildRequires: python3
BuildRequires: pkgconfig(libcrypto)
# install-contract writes the contract directory's package with omx-contract's script
BuildRequires: omx-contract-devel >= 3.4.0

%description
plugin-db is the plugin database, in layers. Today it holds one, the pin: what
identifies the bytes and the parameter layout a host was told to trust, the
SHA-256 of a file, the omx-layout/1 fingerprint, the words a pin is written with
and the rules of what is a pin's.
Its documentation is on openmixer.org.

%package devel
Summary: The pin of a plugin: one header and its pkg-config file
Requires: pkgconfig(libcrypto)

%description devel
<plugin-db/pin.h>, header-only, on libc and libcrypto, and the header of the
words omx-contract renders from data/pin.json, with the pkg-config file
plugin-db.
Its documentation is on openmixer.org.

%package contract
Summary: The words of a plugin pin in omx-contract's contract directory
Requires: omx-contract >= 3.2.0

%description contract
data/pin.json of plugin-db, the verdicts, the pool words, the optional keys,
what counts as changed and the layout formats, as the package plugin-db of
omx-contract's contract directory, under
share/omx-contract/contract.d/3/plugin-db.
Its documentation is on openmixer.org.

%prep
%autosetup -p1

%build

%install
%make_install PREFIX=%{_prefix} INCLUDEDIR=%{_includedir} DATADIR=%{_datadir}
%make_build install-contract DESTDIR=%{buildroot} PREFIX=%{_prefix}

%check
make test

%files devel
%license COPYING
%doc README.md
%dir %{_includedir}/plugin-db
%{_includedir}/plugin-db/pin.h
%{_includedir}/plugin-db/plugin_db_vocabulary.h
%{_datadir}/pkgconfig/plugin-db.pc

%files contract
%license COPYING
%{_datadir}/omx-contract/contract.d/3/plugin-db

%changelog
* Wed Nov 04 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First release: pin.h from plugin-hostd, its SHA-256, the omx-layout/1 fingerprint
  and its records, byte for byte, with the words of a pin declared in data/pin.json.
