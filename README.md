oscmidi
=======

A bidirectionnal bridge between OSC and Midi.
When launched in parallel with Jack, OscMidi will appear in the ALSA tab of QJackCtl.
To compile oscmidi, in addition to 'normal' libraries, you need to have at least these installed:

Alsa library:
 - libasound2
 - libasound2-dev

Lightweight OSC:
 - liblo
 - liblo-dev

Then, to configure and build, just enter the following commands:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RELEASE ..
make
sudo make install
```

OSC messages managed by oscmidi:
 - MIDI Note off          (SND_SEQ_EVENT_NOTEOFF):    "/oscmidi/noteoff"   + <control channel> + <note>  + <velocity>
 - MIDI Note on           (SND_SEQ_EVENT_NOTEON):     "/oscmidi/noteon"    + <control channel> + <note>  + <velocity>
 - MIDI Pressure change   (SND_SEQ_EVENT_KEYPRESS):   "/oscmidi/keypress"  + <control channel> + <note>  + <velocity>
 - MIDI Controller Change (SND_SEQ_EVENT_CONTROLLER): "/oscmidi/cc"        + <control channel> + <param> + <value>
 - MIDI Program Change    (SND_SEQ_EVENT_PGMCHANGE):  "/oscmidi/pgmchange" + <control channel> + <value>
 - MIDI Channel Pressure  (SND_SEQ_EVENT_CHANPRESS):  "/oscmidi/chanpress" + <control channel> + <value>
 - MIDI Pitch bend        (SND_SEQ_EVENT_PITCHBEND):  "/oscmidi/pitchbend" + <control channel> + <value>
 - MIDI Start             (SND_SEQ_EVENT_START):      "/oscmidi/start"     + 1
 - MIDI Continue          (SND_SEQ_EVENT_CONTINUE):   "/oscmidi/continue"  + 1
 - MIDI Start             (SND_SEQ_EVENT_STOP):       "/oscmidi/stop"      + 1

Type of the following parameters:
 - <control channel>: int32
 - <note>: int32
 - <velocity>: int32
 - <value>: int32 or float32 (for Controller Changer, type can be int32 or float32)

Copyright (C) 2011  Jari Suominen (Original author)

Copyright (C) 2014  Yann Collette (CMake + some changes in the code)

