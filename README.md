# Anbox

This code repository is forked from https://github.com/anbox/anbox and 
will be maintained under openEuler android middleware community.We aim 
to run anbox on Arm PC natively.

Anbox is a container-based approach to boot a full Android system on a
regular GNU/Linux system like Ubuntu. In other words: Anbox will let
you run Android on your Linux system without the slowness of
virtualization.

## Overview

Anbox uses Linux namespaces (user, pid, uts, net, mount, ipc) to run a
full Android system in a container and provide Android applications on
any GNU/Linux-based platform.

The Android inside the container has no direct access to any hardware.
All hardware access is going through the anbox daemon on the host. We're
reusing what Android implemented within the QEMU-based emulator for OpenGL
ES accelerated rendering. The Android system inside the container uses
different pipes to communicate with the host system and sends all hardware
access commands through these.

For more details have a look at the following documentation pages:

 * [Android Hardware OpenGL ES emulation design overview](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/android-emugl/DESIGN)
 * [Android QEMU fast pipes](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/docs/ANDROID-QEMU-PIPE.TXT)
 * [The Android "qemud" multiplexing daemon](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/docs/ANDROID-QEMUD.TXT)
 * [Android qemud services](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/docs/ANDROID-QEMUD-SERVICES.TXT)

Anbox is currently suited for the desktop use case but can be used on
mobile operating systems like Ubuntu Touch, Sailfish OS or Lune OS too.
However as the mapping of Android applications is currently desktop specific
this needs additional work to supported stacked window user interfaces too.

The Android runtime environment ships with a minimal customized Android system
image based on the [Android Open Source Project](https://source.android.com/).
The used image is currently based on Android 7.1.1

## Supported Linux Distributions

At the moment we officially support the following Linux distributions:

 * Ubuntu 16.04 (xenial)
 * Ubuntu 18.04 (bionic)
 * UOS

However all other distributions should work as
well as long as they provide the mandatory kernel modules (see kernel/).

 * [Release notes](docs/release-notes/anbox-release-notes.md)


## Build from source

### Requirements

It is recommended that the machine run on the ARM64 architecture.

To build the Anbox runtime itself there is nothing special to know. We're using
cmake as build system. A few build dependencies need to be present on your host
system:

 * libdbus
 * google-mock
 * google-test
 * libboost
 * libboost-filesystem
 * libboost-log
 * libboost-iostreams
 * libboost-program-options
 * libboost-system
 * libboost-test
 * libboost-thread
 * libcap
 * libsystemd
 * mesa (libegl1, libgles2)
 * libsdl2
 * libprotobuf
 * protobuf-compiler
 * lxc (>= 3.0)
 * libasound


On an Ubuntu system you can install all build dependencies with the following
command:

```
$ sudo apt install build-essential cmake cmake-data debhelper dbus google-mock \
    libboost-dev libboost-filesystem-dev libboost-log-dev libboost-iostreams-dev \
    libboost-program-options-dev libboost-system-dev libboost-test-dev \
    libboost-thread-dev libcap-dev libsystemd-dev libegl1-mesa-dev \
    libgles2-mesa-dev libglm-dev libgtest-dev liblxc1 \
    libproperties-cpp-dev libprotobuf-dev libsdl2-dev libsdl2-image-dev lxc-dev \
    pkg-config protobuf-compiler libasound2-dev
```
We recommend Ubuntu 18.04 (bionic) with **GCC 7.x** as your build environment.

On an UOS system you can install all build dependencies with the following
command:

```
$ sudo apt install gcc libncurses-dev bison flex libssl-dev cmake dkms build-essential \
    cmake-data debhelper dbus google-mock libboost-dev libboost-filesystem-dev libboost-log-dev \
    libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libcap-dev \
    libsystemd-dev libegl1-mesa-dev libgles2-mesa-dev libglm-dev libgtest-dev liblxc1 \
    libproperties-cpp-dev libprotobuf-dev libsdl2-dev libsdl2-image-dev lxc-dev libdw-dev \
    libbfd-dev libdwarf-dev pkg-config protobuf-compiler libboost-test-dev
```

### Build

 * [Apply SDL patch](docs/apply_SDL_patch.md)
 * [Install binder & ashmem module](docs/kernel_module.md)

Afterwards you can build Anbox with

```
$ git clone https://gitee.com/openeuler/anbox
$ cd anbox
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_CXX_FLAGS="-DENABLE_TOUCH_INPUT -Wno-error=implicit-fallthrough \
    -Wno-error=missing-field-initializers" -DCMAKE_BUILD_TYPE=Release -DWerror=OFF
$ make -j8

```

If you want to choose a version， please do：

$ git clone https://gitee.com/openeuler/anbox -b anbox-v1.0-rc3

The version anbox-v1.0-rc3 will update in the future， make sure it matches the version of AOSP(see build-android.md).


A simple

```
$ sudo make install
```

will install the necessary bits into your system.

## Run Anbox

Step1：start container manager，run the commands as the ROOT user.

```
$ bash /usr/local/bin/anbox-bridge.sh start
$ anbox container-manager --android-image=/<your path>/android.img \
         --data-path=/<your path>/anbox-data --privileged --daemon &

```

Step2：start session manager，run the commands as the NON-ROOT user， 
and run the shell terminal opened from desktop UI.

```
$ export EGL_PLATFORM=x11
$ export EGL_LOG_LEVEL=fatal
$ anbox session-manager  --gles-driver=host &

```

Wait 30s, open Android APP in "other applications" in the app menu.

## Documentation

You will find additional documentation for Anbox in the *docs* subdirectory
of the project source.

Interesting things to have a look at

 * [Runtime Setup](docs/runtime-setup.md)

## Reporting bugs

If you have found an issue with Anbox, please [file a bug](https://gitee.com/openeuler/anbox/issues).

## Get in Touch

Find maintainer here:
https://gitee.com/openeuler/community/tree/master/sig/sig-android-middleware

## Copyright and Licensing

Anbox reuses code from other projects like the Android QEMU emulator. These
projects are available in the external/ subdirectory with the licensing terms
included.

The Anbox source itself, if not stated differently in the relevant source files,
is licensed under the terms of the GPLv3 license.