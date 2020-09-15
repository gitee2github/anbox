# Build Android Image

It is recommended that all steps in this document be performed on X86 machine with Ubuntu 18.04 (bionic).

 ```
 these android repo is maintained under openEuler android middleware：
 repository1： https://gitee.com/src-openeuler/platform_hardware_libhardware_legacy
 repository2： https://gitee.com/src-openeuler/platform_hardware_ril
 repository3： https://gitee.com/src-openeuler/platform_frameworks_base
 repository4： https://gitee.com/src-openeuler/platform_frameworks_native
 repository5： https://gitee.com/src-openeuler/platform_frameworks_opt_net_wifi
 repository6： https://gitee.com/src-openeuler/platform_system_core
 repository7： https://gitee.com/src-openeuler/platform_packages_apps_DeskClock
 repository8： https://gitee.com/src-openeuler/platform_build
 repository9： https://gitee.com/src-openeuler/platform_manifests
 repository10：https://gitee.com/src-openeuler/platform_packages_apps_PackageInstaller
 ```

For Anbox we're using a minimal customized version of Android but otherwise
base all our work of a recent release of the [Android Open Source Project](https://source.android.com/).

To rebuild the Android image you need first fetch all relevant sources. This
will take quite a huge amount of your disk space (~40GB). AOSP recommends at
least 100GB of free disk space. Have a look at [their](https://source.android.com/source/requirements.html) 
pages too.

In general for building the Anbox Andorid image the instructions found on [the pages
from the AOSP project](https://source.android.com/source/requirements.html) apply.
We will not describe again here of how to build the Android system in general but
only focus on the steps required for Anbox.

On an Ubuntu system you can install all build dependencies with the following command

```
$ apt install -y openjdk-8-jdk libx11-dev libreadline6-dev libgl1-mesa-dev g++-multilib git flex bison gperf build-essential libncurses5-dev tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev  dpkg-dev libsdl1.2-dev git-core gnupg flex bison gperf build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev lib32ncurses5-dev x11proto-core-dev libx11-dev libgl1-mesa-dev libxml2-utils xsltproc unzip m4 lib32z-dev ccache bc python flex bison gperf libsdl-dev build-essential zip curl ninja-build
```

## Fetch all relevant sources

First setup a new workspace where you will download all sources too.

```
$ mkdir $HOME/anbox-work
```

Now initialize the repository by download the manifest and start fetching
the sources:

```
$ cd $HOME/anbox-work
$ repo init -u https://gitee.com/src-openeuler/platform_manifests.git -m default.xml
$ repo sync -d -j4
```

This will take quite some time depending on the speed of your internet connection.

If your network can not touch google，you can replace default.xml with default_tsinghua.xml，like this：

```
$ export REPO_URL='https://mirrors.tuna.tsinghua.edu.cn/git/git-repo'
$ repo init -u https://gitee.com/src-openeuler/platform_manifests.git -m default_tsinghua.xml
$ repo sync -d -j4
```

If you want to choose a version， please do：
```
$ repo init -u https://gitee.com/src-openeuler/platform_manifests.git -m anbox-v1.0-rc3.xml
$ repo sync -d -j4
```
The version anbox-v1.0-rc3 will update in the future， make sure it matches the version of anbox.


## Build Android

When all sources are successfully downloaded you can start building Android itself.

Firstly initialize the environment with the ```envsetup.sh``` script.

```
$ cd $HOME/anbox-work/<android path>
$ . build/envsetup.sh
```

Then initialize the build using ```lunch```.

```
$ lunch anbox_arm64-user
or
$ lunch anbox_arm64-userdebug
```

The complete list of supported build targets:

 * anbox_x86_64-userdebug
 * anbox_armv7a_neon-userdebug
 * anbox_arm64-userdebug

Now build everything with

```
$ export JACK_EXTRA_CURL_OPTIONS=-k
$ export LC_ALL=C
$ make -j8
```

Once the build is done we need to take the results and create an image file
suitable for Anbox.

```
$ cd $HOME/anbox-work/<android path>/vendor/anbox
$ rm -rf android.img （clear the android.img if it has made at last time）
$ scripts/create-package.sh \
    $HOME/anbox-work/<android path>/out/target/product/arm64/ramdisk.img \
    $HOME/anbox-work/<android path>/out/target/product/arm64/system.img
```

This will create an *android.img* file in the current directory.

With this, you are now able to use your custom image within the Anbox runtime.

Copy the *android.img* to the remote ARM machine where anbox run.
