#ifndef SERVO_H
#define SERVO_H

#define SERVO_DEVICE_NAME "robot"
#define SERVO_DRIVER_NAME "servo"

#define SERVO_IOC_SET_DUTY

#define SERVO_IOC_MAGIC 'Q'
#define SERVO_IOC_RESET     _IO(SERVO_IOC_MAGIC, 0)
#define SERVO_IOC_SET_DUTY_NS _IOW(SERVO_IOC_MAGIC, 1, void*)
#define SERVO_IOC_GET_DUTY_NS _IOR(SERVO_IOC_MAGIC, 2, void*)
#define SERVO_IOC_ENABLE _IOW(SERVO_IOC_MAGIC, 3, int)
#define SERVO_IOC_DISABLE _IOW(SERVO_IOC_MAGIC, 4, int)
#define SERVO_IOC_SYNC _IO(SERVO_IOC_MAGIC, 5)
#define SERVO_IOC_MAX 8

#define SERVO_MAJ 0

#define SERVO_CLASS_NAME "servo"

#define SERVO_PWM_PERIOD 20000000

#define TOTAL_NODES 6

struct servo_ioctl_pkt {
	int idx;
	int duty_ns;
};

#endif /* SERVO_H */
