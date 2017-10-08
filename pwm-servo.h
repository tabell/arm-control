#ifndef PWM_SERVO_H
#define PWM_SERVO_H

#define PWM_SERVO_DEVICE_NAME "pwm-servo"

#define SERVO_IOC_MAGIC 'Q'
#define SERVO_IOC_RESET     _IO(SERVO_IOC_MAGIC, 0)
#define SERVO_IOC_SET_STATE _IOW(SERVO_IOC_MAGIC, 1, void*)
#define SERVO_IOC_GET_STATE _IOR(SERVO_IOC_MAGIC, 2, void*)
#define SERVO_IOC_MAX 8

#define SERVO_MAJ 0

#define PWM_SERVO_CLASS_NAME "servos"

struct servo_state {
    int angle;
    int enabled;
};

#endif /* PWM_SERVO_H */
