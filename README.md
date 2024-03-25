# Wayland Scribe
WaylandScribe is a simple program that reads a wayland protocol in xml format and generates C++ code.

## Usage
`wayland-scribe [server|client] specfile [--header-path=<path>] [--prefix=<prefix>] [--add-include=<include>]`


## Dependencies:
* Qt5 or Qt6 (QtCore only)
* C++ compiler with C++17 support

## Notes for compiling - linux:

- Install the dependencies
- Download the sources
  * Git: `git clone https://gitlab.com/marcusbritanicus/wayland-scribe.git WaylandScribe`
- Enter the `WaylandScribe` folder
  * `cd WaylandScribe`
- Configure the project - we use meson for project management
  * `meson setup .build --prefix=/usr --buildtype=release`
- Compile and install - we use ninja
  * `ninja -C .build -k 0 -j $(nproc) && sudo ninja -C .build install`


## Upcoming
* Remove Qt dependency.
* Any other feature you request for... :)
