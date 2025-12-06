Tux Racer Commercial 1.1 Win11 Patch
====================================

Source code for a custom smpeg.dll wrapper that fixes crashes related to music playback in Tux Racer 1.1 on modern Windows systems.

Files:
- smpeg_wrapper.c: The main C source code for the wrapper.
- build.bat:       Build script (requires MinGW).
- smpeg.def:       Export definition file.
- miniaudio.h:     Audio playback library.
- dr_mp3.h:        MP3 decoding library.

How to Build:
1. Ensure MinGW (gcc) is in your PATH.
2. Run `build.bat`.
3. The resulting `smpeg.dll` will be created in this directory.

Does not work on Linux systems and was not tested on versions lower or higher than 1.1.

Changing music options in config.txt isn't recommended as it might cause the game to play music at double speed.

Credits:
- miniaudio by David Reid (https://miniaud.io)
- dr_mp3 by David Reid (https://github.com/mackron/dr_libs)