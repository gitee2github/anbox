# Apply SDL patch

There are some bugs we have to change sdl library or we found it is a better to fix it inside sdl instead of fixing it in anbox.
This patches are here:  _anbox/external/libSDL/_ . You can download sdl library(refer to section "Download SDL") to apply these patches(refer to section "Apply SDL patch") if needed.

## Download SDL

Currently, anbox based on SDL2, so our patches are based on SDL2. You can download the souce package here: 
 _https://www.libsdl.org/release/SDL2-2.0.9.tar.gz_ 

## Apply SDL patch

```
1. Unzip the files you downloaded
2. cd <your dir>/libsdl2-2.0.9+dfsg1
3. patch -p1 < <your dir>/<patch name>.patch
4. dpkg-buildpackage -rfakeroot -b
5. cd ..
6. dpkg -i ./*.deb
```