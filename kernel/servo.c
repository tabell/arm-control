/* ------------------------------------------------------------------------- */
/* System headers */
/* ------------------------------------------------------------------------- */
#include <linux/init.h> /* __init / __exit */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h> /* struct platform_device */
#include <linux/of.h> /* device tree stuff */
#include <linux/slab.h> /* kzalloc */
#include <linux/list.h> /* linked list */
#include <linux/pwm.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

/* ------------------------------------------------------------------------- */
/* Custom headers */
/* ------------------------------------------------------------------------- */
#include "servo.h"

/* ------------------------------------------------------------------------- */
/* Preprocessor macros */
/* ------------------------------------------------------------------------- */
#define pr(fmt, ...) printk(KERN_ERR "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

/* ------------------------------------------------------------------------- */
/* Constants */
/* ------------------------------------------------------------------------- */
#define DEBUG_DEFAULT_DUTY 711000

/* ------------------------------------------------------------------------- */
/* Private data types */
/* ------------------------------------------------------------------------- */
struct servo_state {
    struct list_head list;
    struct pwm_device *pwm;
    int duty_ns;
    bool enabled;
};

/* struct to allocate once for driver */
struct servo_driver_data {
    int major;
    struct class *cl;
    struct device *dev;
    struct pwm_device* servos[TOTAL_NODES];
    struct pwm_state* states[TOTAL_NODES];
};

/* ------------------------------------------------------------------------- */
/* Static global data */
/* ------------------------------------------------------------------------- */

/* Must be exactly TOTAL_NODES entries */
const char* mapping[] = {
    "base",
    "shoulder",
    "elbow",
    "wrist1",
    "wrist2",
    "claw",
};

static struct servo_driver_data *global_data;


LIST_HEAD(node_list);

/* ------------------------------------------------------------------------- */
/* Function declarations */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Function definitions */
/* ------------------------------------------------------------------------- */
static int set_node(
    const char* label,
    struct pwm_device *pwm)
{
    int idx;

    for (idx = 0; idx < TOTAL_NODES; idx++) {
        if (0 == strcmp(mapping[idx], label)) {
            global_data->states[idx] = 
                kzalloc(sizeof(struct pwm_state), GFP_KERNEL); /* TODO: This leaks */
            if (NULL == global_data->states[idx]) {
                return -ENOMEM;
            }
            memcpy(global_data->states[idx],
                    &pwm->state,
                    sizeof(struct pwm_state));
            global_data->servos[idx] = pwm;
            return idx;
        }
    }

    return -ENOENT;
}


/* Called once per device removal */
static int servo_remove(
        struct platform_device *pdev)
{
    pr("Remove %p", pdev);

    return 0;
}

/* Called once per device insertion
 * (or device tree enumeration)
 */
static int servo_probe(
        struct platform_device *pdev)
{
    int ret = 0;
    struct servo_state *new_node;
    int id;

    struct pwm_device *pwm = devm_pwm_get(&pdev->dev, NULL);
    if (IS_ERR(pwm)) {
        pr("Error getting pwm device: %ld", PTR_ERR(pwm));
        return -PTR_ERR(pwm);
    }

    pr("Found servo at %p", pdev);
    new_node = devm_kzalloc(&pdev->dev, 
        sizeof(struct servo_state), GFP_KERNEL);
    if (!new_node) {
        pr("Error allocating memory;");
        ret = -ENOMEM;
        goto err_node_alloc;
    }

#if 0
    pwm_set_period(pwm, SERVO_PWM_PERIOD);
    pwm_set_duty_cycle(pwm, 1000000);
    struct pwm_state test_state;

    test_state.duty_cycle = DEBUG_DEFAULT_DUTY;
    test_state.enabled = true;
    test_state.period = SERVO_PWM_PERIOD;
#endif

    if (0 != (ret = pwm_config(pwm, DEBUG_DEFAULT_DUTY, SERVO_PWM_PERIOD))) {
        pr("pwm_apply_state() failed: %d", ret);
        return ret;
    } else {
        pr("applied config: period=%d duty=%d", SERVO_PWM_PERIOD, DEBUG_DEFAULT_DUTY);
    }

    new_node->pwm = pwm;

    list_add_tail(&new_node->list, &node_list);

    if (0 > (id = set_node(pwm->label, pwm))) {
        pr("was not expecting node %s", pwm->label);
    } else {
        pr("assigned %s to slot %d", pwm->label, id);
    }


    return 0;

err_node_alloc:
    pwm_put(pwm);

    return ret;
}

/* used to match with device tree */
static const struct of_device_id servo_of_match[] = {
    { .compatible = "servo", },
    { },
};

static struct platform_driver servo_platform_driver = {
    .probe = servo_probe,
    .remove = servo_remove,
    .driver = {
        .name = SERVO_DRIVER_NAME,
        .of_match_table = of_match_ptr(servo_of_match),
    },
};

MODULE_DEVICE_TABLE(of, servo_of_match);



static int servo_reset(
    int index)
{
    return 0;
}
static int servo_set_duty_ns(
    int index,
    int duty)
{
    global_data->states[index]->duty_cycle = duty;
    return 0;
}
static int servo_get_duty_ns(
    int index,
    int* duty)
{
    *duty = global_data->states[index]->duty_cycle;
    return 0;
}
static int servo_enable(
    int index)
{
    return 0;
}
static int servo_disable(
    int index)
{
    return 0;
}
static int servo_sync(
    int index)
{
    //return pwm_apply_state(global_data->servos[index], global_data->states[index]);

    return pwm_config(
            global_data->servos[index],
            global_data->states[index]->duty_cycle,
            SERVO_PWM_PERIOD);

}


static long servo_ioctl(
    struct file *file,
    unsigned int num,/* The number of the ioctl */
    unsigned long param) /* The parameter to it */
{
    int ret = 0;
    switch(num) {
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));

    case SERVO_IOC_RESET:
        pr("Unhandled ioctl %d (SERVO_IOC_RESET)", num);
        break;
    case SERVO_IOC_SET_DUTY_NS:
        if (0 != (ret = copy_from_user(&pkt, (void __user *) param, sizeof(pkt)))) {
            printk("%d bytes were not copied into kernel\n", ret);
        } else if (0 != (ret = servo_set_duty_ns(pkt.idx, pkt.duty_ns))) {
            printk("error setting duty");
        }
        break;
    case SERVO_IOC_GET_DUTY_NS:
        if (0 != (ret = copy_from_user(&pkt, (void __user *) param, sizeof(pkt)))) {
            printk("%d bytes were not copied into kernel\n", ret);
        } else if (0 != (ret = servo_get_duty_ns(pkt.idx, &pkt.duty_ns))) {
            printk("error retrieving duty");
        } else if (0 != (ret = copy_to_user((void __user *) param, &pkt, sizeof(pkt)))) {
            printk("%d bytes were not copied out of kernel\n", ret);
        }
        break;
    case SERVO_IOC_ENABLE:
        pr("Unhandled ioctl %d (SERVO_IOC_ENABLE)", num);
        break;
    case SERVO_IOC_DISABLE:
        pr("Unhandled ioctl %d (SERVO_IOC_DISABLE)", num);
        break;
    case SERVO_IOC_SYNC:
        if (0 != (ret = copy_from_user(&pkt, (void __user *) param, sizeof(pkt)))) {
            printk("%d bytes were not copied into kernel\n", ret);
        } else if (0 != (ret = servo_sync(pkt.idx))) {
            printk("Error %d trying to synchronize\n", ret);
        }
        break;

    default:
        pr("not implemented");
        break;
    }
    return ret;
}

struct file_operations fops = {
    .unlocked_ioctl = servo_ioctl,
};

static int __init servo_init(
        void)
{
     /* stack alloc */
    int ret; /* unused */

    /* dynamic alloc */
    pr("Attempting to inititalize driver");
    if (NULL == (global_data = kzalloc(sizeof(struct servo_driver_data ), GFP_KERNEL))) {
        pr("Couldn't allocate memory for driver struct");
        return -ENOMEM;
    }

    /* Register the character device (atleast try) */
    ret = register_chrdev(SERVO_MAJ, SERVO_DEVICE_NAME,
                                 &fops);
    if (ret < 0) {
        pr("Error %d initializing character device", ret);
        goto err_reg_char;
    }
    global_data->major = ret;

    /* Create the class */
    if (IS_ERR(global_data->cl = class_create(THIS_MODULE, SERVO_CLASS_NAME))) {
        pr("Error creating class");
        goto err_class_create;
    }


    /* Create device */
    if (IS_ERR(global_data->dev = device_create(
                    global_data->cl,
                    NULL,
                    MKDEV(global_data->major, 0),
                    NULL,
                    SERVO_DEVICE_NAME))) {
        pr("Error creating device");
        goto err_dev_create;
    }

    /* Register platform driver */
    if (0 > (ret = platform_driver_register(&servo_platform_driver))) {
        pr("Error %d registering platform driver", ret);
       goto err_reg_plat;
    }
    pr("Initialized driver with major number %d", global_data->major);

    return 0;

    /* Dead code (for now) */
    pr("unregistering platform driver");
    platform_driver_unregister(&servo_platform_driver);

err_reg_plat:
    pr("destroying device");
    device_destroy(global_data->cl, MKDEV(global_data->major, 0));

err_dev_create:
    pr("destroying class");
    class_destroy(global_data->cl);

err_class_create:
    pr("unregistering character device");
    unregister_chrdev(global_data->major, SERVO_DEVICE_NAME);

err_reg_char:
    pr("freeing memory");
    kfree(global_data);

    return -EIO;
}

static void __exit servo_exit(
        void)
{
    pr("There");
    pr("unregistering platform driver");
    platform_driver_unregister(&servo_platform_driver);

    pr("destroying device");
    device_destroy(global_data->cl, MKDEV(global_data->major, 0));

    pr("destroying class");
    class_destroy(global_data->cl);

    pr("unregistering character device");
    unregister_chrdev(global_data->major, SERVO_DEVICE_NAME);

    kfree(global_data);
    pr("Done cleanup");
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("alex bell");

module_init(servo_init);
module_exit(servo_exit);
