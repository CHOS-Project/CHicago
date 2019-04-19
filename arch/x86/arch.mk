# File author is Ítalo Lima Marconato Matias
#
# Created on December 20 of 2018, at 10:49 BRT
# Last edited on April 19 of 2019, at 17:48 BRT

arch-clean:
	$(NOECHO)rm -f build/chicago-$(ARCH)_$(SUBARCH).iso

finish: kernel/build/chkrnl-$(ARCH)_$(SUBARCH)
	$(NOECHO)echo Generating build/chicago-$(ARCH)_$(SUBARCH).iso
	$(NOECHO)if [ ! -d build ]; then mkdir -p build; fi
	$(NOECHO)if [ -d build/iso ]; then rm -rf build/iso; fi
	$(NOECHO)mkdir -p build/iso/Boot
	$(NOECHO)cp kernel/build/chkrnl-$(ARCH)_$(SUBARCH) build/iso/Boot/chkrnl.elf
	$(NOECHO)echo '"Boot from CHicago Install CD"=BootDevice,chicago-old' >> build/iso/Boot/bootmgr.conf
	$(NOECHO)echo '"Boot from CHicago Install CD (Verbose)"=BootDevice,chicago-old,verbose' >> build/iso/Boot/bootmgr.conf
	$(NOECHO)dd if=/dev/zero of=build/iso/Boot/bootmgr.img bs=1k count=1440 2>/dev/null
	$(NOECHO)mformat -i build/iso/Boot/bootmgr.img -f 1440 ::
	$(NOECHO)mmd -i build/iso/Boot/bootmgr.img ::/EFI
	$(NOECHO)mmd -i build/iso/Boot/bootmgr.img ::/EFI/BOOT
ifeq ($(SUBARCH),64)
	$(NOECHO)mcopy -i build/iso/Boot/bootmgr.img arch/$(ARCH)/build/bootmgr$(SUBARCH) ::/EFI/BOOT/BOOTX64.EFI
else
	$(NOECHO)mcopy -i build/iso/Boot/bootmgr.img arch/$(ARCH)/build/bootmgr$(SUBARCH) ::/EFI/BOOT/BOOTIA32.EFI
endif
	$(NOECHO)xorriso -as mkisofs -R -c Boot/boot.cat -e Boot/bootmgr.img -U -no-emul-boot -o build/chicago-$(ARCH)_$(SUBARCH).iso build/iso 2>/dev/null
	$(NOECHO)rm -rf build/iso
