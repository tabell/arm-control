#ifndef PWM_SERVO_H
#define PWM_SERVO_H

#define PWM_SERVO_DEVICE_NAME "pwm-servo"

#define SERVO_IOC_MAGIC 'Q'
#define SERVO_IOC_RESET     _IO(SERVO_IOC_MAGIC, 0)
#define SERVO_IOC_SET_ANGLE _IOW(SERVO_IOC_MAGIC, 1, int*)
#define SERVO_IOC_GET_ANGLE _IOR(SERVO_IOC_MAGIC, 2, int*)
#define SERVO_IOC_ENABLE    _IOW(SERVO_IOC_MAGIC, 3, int*)
#define SERVO_IOC_DISABLE   _IOW(SERVO_IOC_MAGIC, 4, int*)
#define SERVO_IOC_MAX 8

#define SERVO_MAJ 0

#define PWM_SERVO_CLASS_NAME "servos"

#endif /* PWM_SERVO_H */
