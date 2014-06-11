oscmidi
=======

A bidirectionnal bridge between OSC and Midi.
When launched in parallel with Jack, OscMidi will appear in the ALSA tab of QJackCtl.

To compile oscmidi, in addition to 'normal' libraries, you need to have at least these installed:

Alsa library:
	libasound2
	libasound2-dev
Lightweight OSC:
	liblo
	liblo-dev

Then, to configure and build, just enter the following commands:

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RELEASE ..
make
sudo make install

Copyright (C) 2011  Jari Suominen (Original author)
Copyright (C) 2014  Yann Collette (CMake + some changes in the code)
