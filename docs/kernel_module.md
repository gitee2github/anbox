1 Anbox-modules源码下载
Anbox-modules代码仓路径: https://github.com/anbox/anbox-modules
下载Anbox-modules代码到/home/compile目录，得到anbox-modules目录
cd /home/compile
git clone https://github.com/anbox/anbox-modules
cd /home/compile/anbox-modules
git reset --hard 816dd4d6e702cf77a44cfe208659af6c39e02b57

2 dkms编译和安装
cd /home/compile/anbox-modules
cp anbox.conf /etc/modules-load.d/
cp 99-anbox.rules /lib/udev/rules.d/
cp -rT ashmem /usr/src/anbox-ashmem-1
cp -rT binder /usr/src/anbox-binder-1
dkms install anbox-ashmem/1
dkms install anbox-binder/1
modprobe ashmem_linux
modprobe binder_linux
确认模块已加载：
lsmod | grep -e ashmem_linux -e binder_linux
预期结果为能够看到2行输出，行首分别为：
binder_linux和ashmem_linux