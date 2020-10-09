# Build and install Android Kernel modules

Install binder and ashmem  with the following command:

```
$ cd /home/compile
$ git clone https://github.com/anbox/anbox-modules
$ cd anbox-modules
$ git reset --hard 816dd4d6e702cf77a44cfe208659af6c39e02b57
$ cp anbox.conf /etc/modules-load.d/
$ cp 99-anbox.rules /lib/udev/rules.d/
$ cp -rT ashmem /usr/src/anbox-ashmem-1
$ cp -rT binder /usr/src/anbox-binder-1
$ dkms install anbox-ashmem/1
$ dkms install anbox-binder/1
$ modprobe ashmem_linux
$ modprobe binder_linux
```

Run the following command to check whether the installation is successful:
```
$ lsmod | grep -e ashmem_linux -e binder_linux
```
If binder_linux and ashmem_linux is listed ， your installation is successful.
