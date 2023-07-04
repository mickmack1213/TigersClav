# TigersClav

TIGERs Cut Lengthy Audio/Video

Video editing tool which understands SSL gamelogs. This tool can process various video formats and load an SSL gamelog along. Videos can be automatically cut based on gamelog events/states. Synchronization between videos and gamelogs can be performed by setting sync markers (e.g. kickoff).

Next to cutting videos, this tool can also generate an overlay video with the current game state.

You may use the output of this tool in your favorite video editing tool to simplify the dull part of cutting out non-running game footage.

This software is in an early stage. Not all video/audio formats may work. You have been warned.

## MSYS2 Setup (Windows)

For windows, install MSYS2 from https://www.msys2.org/

And install the following packages (we are using the UCRT, Universal C Runtime).

```
pacman -S mingw-w64-ucrt-x86_64-toolchain
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ffmpeg
pacman -S mingw-w64-ucrt-x86_64-glfw
pacman -S mingw-w64-ucrt-x86_64-protobuf
```

Add the `msys64\ucrt64\bin` folder to your PATH.

## Linux Setup

Linux build has not been tested yet. The project is based on cmake. Install similar packages to the ones listed in MSYS2 and give it a try.

## Developer Information

The software uses `Dear ImGui` as a cross-platform GUI front-end with GLFW as back-end. Video and audio decoding/encoding is done via `ffmpeg` and supports hardware decoders/encoders to improve performance.
