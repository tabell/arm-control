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
/*  macros */
/* ------------------------------------------------------------------------- */
#define pr_dbg(fmt, ...) printk(KERN_DEBUG "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define pr(fmt, ...) printk(KERN_NOTICE "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define prerr(fmt, ...) printk(KERN_ERR "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define ELEMS(x) (sizeof(x) / sizeof((x)[0]))

/* ------------------------------------------------------------------------- */
/* Constants */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Static data */
/* ------------------------------------------------------------------------- */
static const char* joints[] = {
    "base",
    "shoulder",
    "elbow",
    "wrist1",
    "wrist2",
    "claw",
};
#define TOTAL_NODES ELEMS(joints)

static struct servo_driver_data *global_data;


LIST_HEAD(node_list);

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
/* Function declarations */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Function definitions */
/* ------------------------------------------------------------------------- */
/* Gets called once to configure a new axis */
/* Returns positive ID or negative errno */
static int store_servo_info(
    const char* label,
    struct pwm_device *pwm)
{
    int idx;

    for (idx = 0; idx < TOTAL_NODES; idx++) {
        if (0 == strcmp(joints[idx], label)) {
            global_data->states[idx] = kzalloc(sizeof(struct pwm_state), GFP_KERNEL); /* TODO: This leaks */
            if (NULL == global_data->states[idx]) {
                return -ENOMEM;
            }
            memcpy(global_data->states[idx], &pwm->state, sizeof(struct pwm_state));
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
    pr_dbg("Removing %s", pdev->name);
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
    struct pwm_device *pwm;
    int id;

    pr_dbg("Probing for %s", pdev->name);
    pwm = devm_pwm_get(&pdev->dev, NULL);
    if (IS_ERR(pwm)) {
        prerr("Error getting pwm device: %ld", PTR_ERR(pwm));
        return -PTR_ERR(pwm);
    }

    pr_dbg("Found %s", pdev->name);
    new_node = devm_kzalloc(&pdev->dev, sizeof(struct servo_state), GFP_KERNEL);
    if (!new_node) {
        pr("Error allocating memory;");
        ret = -ENOMEM;
        goto err_node_alloc;
    }

    new_node->pwm = pwm;

    list_add_tail(&new_node->list, &node_list);

    if (0 > (id = store_servo_info(pwm->label, pwm))) {
        pr("was not expecting node %s", pwm->label);
    } else {
        pr_dbg("assigned %s to slot %d", pwm->label, id);
        pwm_disable(pwm);
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



static int servo_set_duty_ns(
    int index,
    int duty)
{
    global_data->states[index]->duty_cycle = duty;
    return 0;
}
static int servo_get_duty_ns(
    unsigned char index,
    int* duty)
{
    *duty = global_data->states[index]->duty_cycle;
    pr_dbg("got index=%d duty=%d", index, *duty);
    return 0;
}

static int servo_sync(
    int index)
{
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
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (0 != (ret = copy_from_user(&pkt, (void __user *) param, sizeof(pkt)))) {
        if (ret > 0) {
            prerr("%d bytes were not copied into kernel\n", ret);
        } else {
            prerr("error %d copying from user space\n", ret);
        }
    } else {
        switch(num) {

            case SERVO_IOC_RESET:
                prerr("Unhandled ioctl %d (SERVO_IOC_RESET)", num);
                break;
            case SERVO_IOC_SET_DUTY_NS:
                if (0 != (ret = servo_set_duty_ns(pkt.idx, pkt.duty_ns))) {
                    prerr("error setting duty");
                } else {
                    pr_dbg("id: %d duty: %d", pkt.idx, pkt.duty_ns);
                }
                break;
            case SERVO_IOC_GET_DUTY_NS:
                if (!ret && 0 != (ret = servo_get_duty_ns(pkt.idx, &pkt.duty_ns))) {
                    prerr("error retrieving duty");
                }

                if (!ret && 0 != (ret = copy_to_user((void __user *) param, &pkt, sizeof(pkt)))) {
                    prerr("%d bytes were not copied out of kernel\n", ret);
                } else {
                    pr_dbg("id: %d duty: %d", pkt.idx, pkt.duty_ns);
                }
            case SERVO_IOC_ENABLE:
                if (0 != (ret = pwm_enable( global_data->servos[pkt.idx]))) {
                    prerr("error %d enabling servo %d", ret, pkt.idx);
                }
                break;
            case SERVO_IOC_DISABLE:
                pwm_disable(global_data->servos[pkt.idx]);
                break;
            case SERVO_IOC_SYNC:
                if (0 != (ret = servo_sync(pkt.idx))) {
                    prerr("Error %d trying to synchronize\n", ret);
                }
                break;

            default:
                prerr("not implemented");
                break;
        }
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
        prerr("Couldn't allocate memory for driver struct");
        return -ENOMEM;
    }
    pr("allocated memory for driver struct");

    /* Register the character device (atleast try) */
    ret = register_chrdev(SERVO_MAJ, SERVO_DEVICE_NAME,
                                 &fops);
    if (ret < 0) {
        prerr("Error %d initializing character device", ret);
        goto err_reg_char;
    }
    pr("initialized character device");
    global_data->major = ret;

    /* Create the class */
    if (IS_ERR(global_data->cl = class_create(THIS_MODULE, SERVO_CLASS_NAME))) {
        prerr("Error creating class");
        goto err_class_create;
    }
    pr("created class");


    /* Create device */
    if (IS_ERR(global_data->dev = device_create(
                    global_data->cl,
                    NULL,
                    MKDEV(global_data->major, 0),
                    NULL,
                    SERVO_DEVICE_NAME))) {
        prerr("Error creating device");
        goto err_dev_create;
    }

    /* Register platform driver */
    if (0 > (ret = platform_driver_register(&servo_platform_driver))) {
       prerr("Error %d registering platform driver", ret);
       goto err_reg_plat;
    }
    pr_dbg("Initialized driver with major number %d", global_data->major);

    return 0;

    /* Dead code (for now) */
    pr_dbg("unregistering platform driver");
    platform_driver_unregister(&servo_platform_driver);

err_reg_plat:
    pr_dbg("destroying device");
    device_destroy(global_data->cl, MKDEV(global_data->major, 0));

err_dev_create:
    pr_dbg("destroying class");
    class_destroy(global_data->cl);

err_class_create:
    pr_dbg("unregistering character device");
    unregister_chrdev(global_data->major, SERVO_DEVICE_NAME);

err_reg_char:
    pr_dbg("freeing memory");
    kfree(global_data);

    return -EIO;
}

static void __exit servo_exit(
        void)
{
    pr_dbg("unregistering platform driver");
    platform_driver_unregister(&servo_platform_driver);

    pr_dbg("destroying device");
    device_destroy(global_data->cl, MKDEV(global_data->major, 0));

    pr_dbg("destroying class");
    class_destroy(global_data->cl);

    pr_dbg("unregistering character device");
    unregister_chrdev(global_data->major, SERVO_DEVICE_NAME);

    kfree(global_data);
    pr_dbg("freed global data");
    return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("alex bell");

module_init(servo_init);
module_exit(servo_exit);
