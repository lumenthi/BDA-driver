NAME = driver

obj-m += $(NAME).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f api_tests

load: all
	sudo rmmod usbhid || true # All USB HID devices are managed by usbhid driver, we must remove it.
	sudo insmod $(NAME).ko

unload:
	sudo rmmod -f $(NAME)
	# sudo insmod /lib/modules/4.19.0-18-amd64/kernel/drivers/hid/usbhid/usbhid.ko
	# find /lib/modules/$(uname -r)/ -iname '*usbhid*'	

reload: unload all load

log:
	sudo dmesg --level=debug --follow

read:
	sudo cat sudo cat /dev/input/js0 | xxd -b

main:
	gcc -pthread main.c controller.c -o api_tests && ./api_tests

jstest:
	jstest /dev/input/js0
