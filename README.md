# arm-control

## Intro

This is only designed to work with my unique hardware set up which consists of six PWM servos connected to an external PCA9685, which is connected to the I2C bus on pins 2/3 of the raspberry pi 2B

## Prereqs

Kernel headers must be installed to build kernel module
device-tree-compiler must be installed to build dts
gcc toolchain must be installed to build kernel and userspace

## Instructions

### Building

```bash
cd dts
make
cd ../kernel
make
cd ../user
make
```

### Use

```bash
sudo dtoverlay dts/robot.dtbo
sudo insmod kernel/servo.ko
sudo chmod 666 /dev/robot
user/sweep <idx> [<angle>]
```

