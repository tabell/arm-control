obj-m+=servo.o
KERNELVER=4.9.35+
servo.ko: servo.c servo.h
	    make -C /lib/modules/$(KERNELVER)/build M=${PWD} modules

clean:
	    make -C /lib/modules/$(KERNELVER)/build M=${PWD} clean
