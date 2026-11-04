# plugin-pin

The pin of a plugin: what identifies the bytes and the parameter layout a host was told to trust, the words a pin is
written with, and the rules of what is a pin's and what is not.

`<plugin-pin/pin.h>` is one header on libc and libcrypto:

- the SHA-256 of a file over an open descriptor;
- `omx-layout/1`, the fingerprint of a parameter layout, from neutral records a host fills from CLAP params or LV2
  control ports (the header's first comment is the format, line by line);
- whether a digest, a layout pin `<scheme>:<sha256>`, a verdict or a pool word is valid.

`data/pin.json` declares the words: the verdicts, the pool words, the optional keys, what counts as changed and the
layout formats. omx-contract renders them to `<plugin-pin/plugin_pin_vocabulary.h>`, and the package `plugin-pin-contract` puts them in
its contract directory. A change to the serialisation is a new scheme, never an edit of this one.

This repository looks inside no plugin. The host fills the records; scanning and loading belong to the door that runs
them. The pin store and the `pin_*` verbs belong to plugin-hostd and plugin-hostd-protocol.

## Install

Fedora 44:

```
sudo dnf install https://www.openmixer.org/rpm/openmixer-release.noarch.rpm
sudo dnf install plugin-pin-devel
```

Debian bookworm and trixie (also Raspberry Pi OS and Zynthian):

```
curl -fsSLO https://www.openmixer.org/deb/openmixer-release.deb && sudo apt install ./openmixer-release.deb
sudo apt install plugin-pin-dev
```

## Start

```
cc $(pkg-config --cflags plugin-pin) your.c $(pkg-config --libs plugin-pin)
```

## Build and test

```
make test
make install PREFIX=/usr/local
```

`make test` needs a C compiler, python3 and libcrypto's headers, and no plugin. `make check-tools` also holds the
rendered header to `data/` and validates the package against the contract directory; it needs omx-contract and
omx-contract-devel.

## Licence

GPL-3.0-or-later. See [COPYING](COPYING).
