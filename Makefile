TARGET_MODULE:=frandom

# If we running by kernel building system
ifneq ($(KERNELRELEASE),)
 $(TARGET_MODULE)-objs := src/main.o src/frandom.o
 obj-m := $(TARGET_MODULE).o

# If we are running without kernel build system
else
 BUILDSYSTEM_DIR?=/lib/modules/$(shell uname -r)/build
 PWD:=$(shell pwd)


all : 
# run kernel build system to make module
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
# run kernel build system to cleanup in current directory
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko

endif
