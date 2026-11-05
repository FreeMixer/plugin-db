Name: plugin-db
Version: 0.2.0
Release: 1%{?dist}
License: GPL-3.0-or-later
Summary: The plugin database: the pin, the address form, the store and the state
URL: https://github.com/FreeMixer/plugin-db

Source0: %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: make
BuildRequires: pkgconfig
BuildRequires: python3
BuildRequires: pkgconfig(libcrypto)
# install-contract writes the contract directory's package with omx-contract's script
BuildRequires: (omx-contract-devel >= 3.4.0 with omx-contract-devel < 4)

%description
plugin-db is the plugin database, in layers: the pin of a plugin, the one
address form, which files are a plugin (the LV2 bundle rule included), the
store of layers and the one state function. The command plugin-db pins an
entry from its files, shows one entry's attributes, state and reason, and
verifies every entry as rpm -V does. It loads no plugin.
Its documentation is on openmixer.org.

%package devel
Summary: The plugin database: headers, a static library and its pkg-config file
Requires: pkgconfig(libcrypto)

%description devel
<plugin-db/pin.h>, header-only, on libc and libcrypto; <plugin-db/db.h> and
libplugin-db.a, the address form, the file collection, the store and the state
function; the headers of the words and tables rendered from data/, with the
pkg-config file plugin-db.
Its documentation is on openmixer.org.

%package contract
Summary: The words of the plugin database in omx-contract's contract directory
Requires: (omx-contract >= 3.2.0 with omx-contract < 4)

%description contract
data/pin.json of plugin-db, the words of a pin, and data/db.json, the address
formats, the bundle rule, the layers, the attributes, the record changes and
their actions, the states and the reasons, as the package plugin-db of
omx-contract's contract directory, under
share/omx-contract/contract.d/3/plugin-db.
Its documentation is on openmixer.org.

%prep
%autosetup -p1

%build
%make_build CFLAGS="%{optflags}"

%install
%make_install PREFIX=%{_prefix} BINDIR=%{_bindir} INCLUDEDIR=%{_includedir} LIBDIR=%{_libdir} DATADIR=%{_datadir}
%make_build install-contract DESTDIR=%{buildroot} PREFIX=%{_prefix}

%check
make test

%files
%license COPYING
%doc README.md
%{_bindir}/plugin-db
%dir %{_datadir}/plugin-db
%{_datadir}/plugin-db/store.schema.json

%files devel
%license COPYING
%doc README.md
%dir %{_includedir}/plugin-db
%{_includedir}/plugin-db/pin.h
%{_includedir}/plugin-db/db.h
%{_includedir}/plugin-db/plugin_db_vocabulary.h
%{_includedir}/plugin-db/plugin_db_tables.h
%{_libdir}/libplugin-db.a
%{_datadir}/pkgconfig/plugin-db.pc

%files contract
%license COPYING
%{_datadir}/omx-contract/contract.d/3/plugin-db

%changelog
* Wed Nov 04 2026 Pau Aliagas <linuxnow@gmail.com> - 0.2.0-1
- The address form, the file collection with the LV2 bundle rule (newest-version),
  the store of layers, the state function, and the command plugin-db with pin, put,
  show and verify; declared in data/db.json and data/store.schema.json.
* Wed Nov 04 2026 Pau Aliagas <linuxnow@gmail.com> - 0.1.0-1
- First release: pin.h from plugin-hostd, its SHA-256, the omx-layout/1 fingerprint
  and its records, byte for byte, with the words of a pin declared in data/pin.json.
