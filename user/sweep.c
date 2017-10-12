#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "servo.h"

#define DEF_DUTY 900000

int fd = -1;
const char *path = "/dev/robot";

typedef enum command {
    COMMAND_UNKNOWN,
    CMD_ON,
    CMD_OFF,
    CMD_SET,
    CMD_GET,
    CMD_SYNC,
} tCmd;


typedef struct node {
    int index;
    int min_duty;
    int max_duty;
    int duty;
    int a;
    int b;
} tNode;

tNode node[] = {{ 0, 600000, 2400000, DEF_DUTY, 10000, 0 },
                { 1, 600000, 2600000, DEF_DUTY, 10000, 0 },
                { 2, 200000, 2400000, DEF_DUTY, 10000, 0 },
                { 3, 200000, 2400000, DEF_DUTY, 10000, 0 },
                { 4, 200000, 2400000, DEF_DUTY, 10000, 0 },
                { 5, 200000, 2400000, DEF_DUTY, 10000, 0 }};

int setDuty(tNode* node, int duty)
{
    if (duty > node->max_duty) duty = node->max_duty;
    if (duty < node->min_duty) duty = node->min_duty;
    struct servo_ioctl_pkt pkt;
    //printf("before duty = %d ", node->duty);
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;
    pkt.duty_ns = duty;

    if (0 != (ioctl(fd, SERVO_IOC_SET_DUTY_NS, &pkt))) {
        printf("Error %d calling set ioctl: %s\n", errno, strerror(errno));
        return -errno;
    }
    node->duty = duty;
    //printf("after duty = %d\n", node->duty);
    return 0;
}

int getDuty(tNode* node)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;

    if (0 != (ioctl(fd, SERVO_IOC_GET_DUTY_NS, &pkt))) {
        return -errno;
    }
    node->duty = pkt.duty_ns;
    return 0;

}

int servoSync(tNode* node)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;

    if (0 != (ioctl(fd, SERVO_IOC_SYNC, &pkt))) {
        return -errno;
    }
    return 0;


}

int sweep(tNode *node, int duty_end)
{
    int ret = 0;

    int duty_start = node->duty;

    if (duty_start == duty_end) {
        return 0;
    }

    fprintf(stderr, "sweeping node %d from %d to %d in %f s\n",
            node->index, duty_start, duty_end);

    
    int duty_delta = duty_end - duty_start;
    int num_steps = abs(duty_delta / 4000);

    int duty_step = duty_delta / num_steps;
    float duration = abs(duty_delta / 400000);
    float time_delta = (duration * 1E9);
    int time_step = time_delta  / num_steps;
    fprintf(stderr, "steps = %d, duty step = %d, time ns step = %d ms, duration = %.2f s\n",
            num_steps, duty_step, time_step / 1000000, ((float)time_step * num_steps) / 1E9);

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = abs(time_step),
    };


        int duty = duty_start;
        fprintf(stderr, "start = %d goal = %d step = %d\n",
                duty, duty_end, duty_step);

        int step = 0;
        //for (duty = duty_start; abs(duty - duty_end) > abs(duty_step); duty += duty_step) {
        for (step = 0; step < num_steps; step++) {
            duty += duty_step;
#ifdef DEBUG_PRINT
            if (step++ % 20 == 0) {
                fprintf(stderr, "%d: start = %d current = %d end = %d step = %d\n",
                        step, duty_start, duty, duty_end, duty_step);
            }
#endif
            if (0 != (ret = setDuty(node, duty))) {
                fprintf(stderr, "Error %d setting duty %d for id %d: %s\n",
                        ret, duty, index, strerror(-ret));
            } else if (0 != (ret = servoSync(node))) {
                fprintf(stderr, "Error %d syncing for id %d: %s\n",
                        ret, index, strerror(-ret));
            } else if (0 != (ret = nanosleep(&delay, NULL))) {
                fprintf(stderr, "error %d calling nanosleep: %s\n",
                        ret, strerror(-ret));
            }

            if (ret) break;
        }


    return ret;
}

int main(int argc, char** argv) {
    /* Allocation */
    int ret;
    int index = -1;
    int duty_end = -1;
    bool setting = false;

    /* Parse arguments */
    if (argc > 1) {
        /* parse index */
        index = strtoul(argv[1], NULL, 10);
        if ((index > 5) || (index < 0)) {
            fprintf(stderr,"index out of range\n");
            return 0;
        }
    } else {
        fprintf(stderr,
                "usage: %s <index 1-6> [<duty>]\n", argv[0]);
        return 0;
    }

    if (argc > 2) {
        duty_end = strtoul(argv[2], NULL, 10);
        setting = true;
    }


    fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening %s: %s\n",
                fd, path, strerror(fd));
    } else {
        if (0 != (ret = getDuty(&node[index]))) {
            printf("getDuty returns %d: %s",
                    -ret, strerror(-ret));
        } else {
            if (setting) {
                sweep(&node[index], duty_end * node[index].a + node[index].b);
            }
            printf("duty: %d\n", (node[index].duty - node[index].b) / node[index].a);
        }
        close(fd);
    }
    return 0;
}
