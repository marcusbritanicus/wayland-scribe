# Wayland Scribe
WaylandScribe is a simple program that reads a wayland protocol in xml format and generates C++ code.

## Usage
`wayland-scribe --[server|client] specfile [--header-path=<path>] [--prefix=<prefix>] [--add-include=<include>]`

There are two ways to use the code generated by WaylandScribe.
1. Modify the code generated directly, by editing the cpp file (and if needed the hpp file), and create instances of them. Only the methods
   marked virtual will need to be changed.
2. Alternatively, these can be sub-classed, and the virtual methods can be over-ridden. The subclasses can then be used in the code.

Both methods have their own pros and cons.
In method 1, once the code is generated, wayland-scribe is no longer required. The source and the header files can be installed or uploaded to
your code repo, and everyone can use it without issues. The disadvantage however is that if the protocol changes significantly, updating the
classes can be cumbersome.

Alternatively, in method 2, since the parent classes are generated dynamically, and only a few functions will need changes in the child classes,
maintenance is simplified. However, if you do not upload these dynamically generated files to the repo, you'll make wayland-scribe a hard-depend
of your project.

Apart from these two differences, the two methods are identical and are not expected to show any difference in performance.

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
