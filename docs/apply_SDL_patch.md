# Apply SDL patch

There are some bugs we have to change sdl library or we found it is a better to fix it inside sdl instead of fixing it in anbox.
This patches are here:  _anbox/external/libSDL/_ . You can download sdl library(refer to section "Download SDL") to apply these patches(refer to section "Apply SDL patch") if needed.

## Download SDL

Currently, anbox based on SDL2, so our patches are based on SDL2. You can download the souce package here: 
 _https://www.libsdl.org/release/SDL2-2.0.9.tar.gz_ 

## Apply SDL patch

```
$ mkdir -p /home/compile/sdl
$ cd /home/compile/sdl
$ apt-get source libsdl2-2.0-0
$ apt-get install fakeroot dpkg-dev build-essential
$ apt-get build-dep libsdl2-2.0-0
$ cd libsdl2-2.0.9+dfsg1
$ patch -p1 < /home/compile/anbox/external/libSDL/SDL_fix_Chinese_input.patch
$ patch -p1 < /home/compile/anbox/external/libSDL/SDL_fix_clipboard_crash_issuse.patch
$ patch -p1 </home/compile/anbox/external/libSDL/SDL_fix_Restore.patch
$ dpkg-buildpackage -rfakeroot -b
$ cd ..
$ dpkg -i ./*.deb
```