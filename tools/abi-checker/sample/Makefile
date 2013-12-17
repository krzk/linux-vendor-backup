obj-m = sample_module.o

all:
	make -C /usr/src/linux-kernel-build-3.10.19-tizen_defconfig.1 M=$(PWD) modules

install:
	make -C /usr/src/linux-kernel-build-3.10.19-tizen_defconfig.1 M=$(PWD) modules_install

clean:
	make -C /usr/src/linux-kernel-build-3.10.19-tizen_defconfig.1 M=$(PWD) clean
	

