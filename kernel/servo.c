/* System headers */
#include <linux/init.h> /* __init / __exit */
#include <linux/kernel.h>
#include <linux/module.h>

#define pr(fmt, ...) printk(KERN_ERR "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)


static struct platform_driver servo = {
    .probe = servo_probe,
    .remove = servo_remove,
    .driver = {
        .name = SERVO_DRIVER_NAME,
        .of_match_table = of_match_ptr(servo_of_match),
    },
};

static int __init pwm_servo_init(
        void)
{
    pr("Here");
     /* alloc */
#if 0
    int ret;
    pr("Attempting to inititalize driver");
    if (NULL == (drv_data = kzalloc(sizeof(struct pwm_servo_drv_data ), GFP_KERNEL))) {
        pr("Couldn't allocate memory for driver struct");
        return -ENOMEM;
    }

    /* Register the character device (atleast try) */
    ret = register_chrdev(SERVO_MAJ, PWM_SERVO_DEVICE_NAME,
                                 &fops);
    if (ret < 0) {
        pr("Error %d initializing character device", ret);
        goto err_reg_char;
    }
    drv_data->major_number = ret;

    /* Create the class */
    if (IS_ERR(drv_data->cl = class_create(THIS_MODULE, PWM_SERVO_CLASS_NAME))) {
        pr("Error creating class");
        goto err_class_create;
    }


    /* Create device */
    if (IS_ERR(drv_data->dev = device_create(
                    drv_data->cl,
                    NULL,
                    MKDEV(drv_data->major_number, 0),
                    NULL,
                    PWM_SERVO_DEVICE_NAME))) {
        pr("Error creating device");
        goto err_dev_create;
    }

    /* Register platform driver */
    if (0 > (ret = platform_driver_register(&pwm_servo_driver))) {
        pr("Error %d registering platform driver", ret);
        goto err_reg_plat;
    }

    pr("Initialized driver with major number %d", drv_data->major_number);

    return 0;

    /* Dead code (for now) */
    pr("unregistering platform driver");
    platform_driver_unregister(&pwm_servo_driver);

err_reg_plat:
    pr("destroying device");
    device_destroy(drv_data->cl, MKDEV(drv_data->major_number, 0));

err_dev_create:
    pr("destroying class");
    class_destroy(drv_data->cl);

err_class_create:
    pr("unregistering character device");
    unregister_chrdev(drv_data->major_number, PWM_SERVO_DEVICE_NAME);

err_reg_char:
    pr("freeing memory");
    kfree(drv_data);

    return -EIO;
#endif
    return 0;
}

static void __exit pwm_servo_exit(
        void)
{
    pr("There");
#if 0
    pr("unregistering platform driver");
    platform_driver_unregister(&pwm_servo_driver);

    pr("destroying device");
    device_destroy(drv_data->cl, MKDEV(drv_data->major_number, 0));

    pr("destroying class");
    class_destroy(drv_data->cl);

    pr("unregistering character device");
    unregister_chrdev(drv_data->major_number, PWM_SERVO_DEVICE_NAME);

    kfree(drv_data);
    pr("Done cleanup");
#endif
    return;
}

module_init(pwm_servo_init);
module_exit(pwm_servo_exit);
