# TigersClav

TIGERs Cut Lengthy Audio/Video

Video editing tool which understands SSL gamelogs.

## MSYS2 Setup

For windows, install MSYS2 from https://www.msys2.org/

And install the following packages (we are using the UCRT, Universal C Runtime).

```
pacman -S mingw-w64-ucrt-x86_64-toolchain
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ffmpeg
pacman -S mingw-w64-ucrt-x86_64-glfw
```

Add the `msys64\ucrt64\bin` folder to your PATH.
