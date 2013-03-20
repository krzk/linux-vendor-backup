human_arch	= ARM (hard float)
build_arch	= arm
header_arch	= arm
defconfig	= defconfig
#flavours	= generic highbank
flavours	= generic
build_image	= zImage
kernel_file	= arch/$(build_arch)/boot/zImage
install_file	= vmlinuz
no_dumpfile	= true

loader		= grub
do_tools	= false

# Flavour specific configuration.
#dtb_file_highbank	= arch/$(build_arch)/boot/highbank.dtb
dtb_files_generic	= imx6q-sabrelite.dtb omap3-beagle-xm.dtb omap4-panda.dtb omap4-panda-es.dtb
