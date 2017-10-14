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
#include <math.h>

#include "servo.h"

#define DEF_DUTY 900000

//#define DRY_RUN
//#define DEBUG_PRINT
#define  DUTY_STEP 4000

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

typedef float (*tPathFunc)(float);

typedef struct path {
    int start_duty;
    int target_duty;
    float progress;
    tPathFunc path_func; 
} tPath;

typedef struct node {
    int index;
    int min_duty;
    int max_duty;
    int duty;
    int a;
    int b;
    tPath *path;
} tNode;

tNode node[] = {{ 0, 600000, 2400000, DEF_DUTY, 10000, 0 },
                { 1, 600000, 2600000, DEF_DUTY, 10000, 0 },
                { 2, 600000, 2400000, DEF_DUTY, 10000, 0 },
                { 3, 600000, 2400000, DEF_DUTY, 10000, 0 },
                { 4, 600000, 2400000, DEF_DUTY, 10000, 0 },
                { 5, 600000, 2400000, DEF_DUTY, 10000, 0 }};

float gentle(float in)
{
    return logf(in);
}

float euler_poisson(float x)
{
    float two_x = 2*x;
    return 1-expf(-two_x*two_x);
}

float gentle2(float x)
{
    float s = sinf(8*x/5);
    return s*s;
}

int calcNextDuty(tNode* node)
{
    if (!node || !node->path) {
        return -EINVAL;
    }

    int base = node->path->start_duty;
    int delta = node->path->target_duty - node->path->start_duty;
    int step = 0;

    if (!node->path->path_func) {
        step = delta * node->path->progress;
    } else {
        step = delta * node->path->path_func(node->path->progress);
    }
    node->duty = base + step;
    return 0;
}

int setAllDuty(tNode *nodes[], float progress)
{

    int ret = 0;
    int i;
    for (i = 0; i < 6; i++) {
        if (!nodes[i]) {
            printf("error node %d null\n", i);
            return -EINVAL;
        }
        nodes[i]->path->progress = progress;
        if (0 != (ret = calcNextDuty(nodes[i]))) {
            printf("Error %d setting node %d: %s\n",
                    -ret, i, strerror(-ret));
            break;
        } else if (0 != (ret = setDuty(nodes[i]))) {
            printf("Error %d setting node %d: %s\n",
                    -ret, i, strerror(-ret));
            break;
        }
    }
    return ret;
}

int setDuty(tNode* node)
{
    if (node->duty > node->max_duty) node->duty = node->max_duty;
    if (node->duty < node->min_duty) node->duty = node->min_duty;
    struct servo_ioctl_pkt pkt;
    //printf("before duty = %d ", node->duty);
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;
    pkt.duty_ns = node->duty;
#ifndef DRY_RUN
    if (0 != (ioctl(fd, SERVO_IOC_SET_DUTY_NS, &pkt))) {
        printf("Error %d calling set ioctl: %s\n", errno, strerror(errno));
        return -errno;
    }
#endif
    return 0;
}

int getDuty(tNode* node)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;
#ifndef DRY_RUN
    if (0 != (ioctl(fd, SERVO_IOC_GET_DUTY_NS, &pkt))) {
        return -errno;
    }
#endif
    node->duty = pkt.duty_ns;
    return 0;

}

int servoSync(tNode* node)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;

#ifndef DRY_RUN
    if (0 != (ioctl(fd, SERVO_IOC_SYNC, &pkt))) {
        return -errno;
    }
#endif
    return 0;


}

int sweep(tNode *node, int duty_end)
{
    int ret = 0;

    int duty_start = node->duty;

    if (abs(duty_start - duty_end) < 100) {
        return 0;
    }
#ifdef DEBUG_PRINT
    fprintf(stderr, "sweeping node %d from %d to %d\n",
            node->index, duty_start, duty_end);
#endif
    
    int duty_delta = duty_end - duty_start;
    printf("duty_delta = duty_end (%d) - duty_start(%d) = %d\n", duty_end, duty_start, duty_delta);
    int num_steps = abs(duty_delta / 3999);
    printf(" num_steps = abs(duty_delta / 3999) = %d\n", num_steps);
    float progress_step = (float)DUTY_STEP / (float)duty_delta;
    printf("progress_step = duty_step / duty_delta = %f\n", progress_step);
    float duration = fabs((float)duty_delta / (float)1800000);
    printf("duration = abs(duty_delta / 1800000) = %f\n", duration);
    float time_delta = (duration * 1E9);
    printf("time_delta = (duration * 1E9) = %d\n", time_delta);
#if 0
    fprintf(stderr, "steps = %d, duty step = %d, time ns step = %d ms, duration = %.2f s\n",
            num_steps, duty_step, time_step / 1000000, ((float)time_step * num_steps) / 1E9);
#endif
    int time_step = time_delta  / num_steps;
    printf("time_step = time_delta  / num_steps = %d\n", time_step);
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = abs(time_step),
    };


    int duty = duty_start;

    tPath path = { duty_start, duty_end, 0, NULL };
    path.path_func = gentle2;

    node->path = &path;

    int step = 0;
    //for (duty = duty_start; abs(duty - duty_end) > abs(duty_step); duty += duty_step) {
    for (step = 0; step < num_steps; step++) {
        node->path->progress += progress_step;
        calcNextDuty(node);
        if (0 != (ret = setDuty(node))) {
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
        printf("Error %d opening %s: %s\n",
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
