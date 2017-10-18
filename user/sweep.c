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
#include <time.h> /* struct timespec and nanosleep */
#include <assert.h>

#include "servo.h"

#define DEF_DUTY 900000

#define pr(fmt, ...) fprintf(stderr, "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

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
    float progress_step;
    int num_steps;
    tPathFunc path_func; 
} tPath;

typedef struct node {
    int index;
    int min_duty;
    int max_duty;
    int duty;
    int last_duty;
    int a;
    int b;
    tPath *path;
} tNode;

tNode g_node[] = {{   .index = 0, .min_duty = 600000, .max_duty = 2400000, .duty = DEF_DUTY, .a = 10000, .b = 0 },
                    { .index = 1, .min_duty = 600000, .max_duty = 2600000, .duty = DEF_DUTY, .a = 10000, .b = 0 },
                    { .index = 2, .min_duty = 600000, .max_duty = 2400000, .duty = DEF_DUTY, .a = 10000, .b = 0 },
                    { .index = 3, .min_duty = 600000, .max_duty = 2400000, .duty = DEF_DUTY, .a = 10000, .b = 0 },
                    { .index = 4, .min_duty = 600000, .max_duty = 2400000, .duty = DEF_DUTY, .a = 10000, .b = 0 },
                    { .index = 5, .min_duty = 1800000, .max_duty = 2400000, .duty = DEF_DUTY, .a = 10000, .b = 0 }};

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
    node->last_duty = node->duty;
    node->duty = base + step;
    //pr( "diff = %d", node->duty - node->last_duty);
    return 0;
}
#if 0
int setAllDuty(tNode *nodes[], float progress)
{

    int ret = 0;
    int i;
    for (i = 0; i < 6; i++) {
        if (!nodes[i]) {
            pr("error node %d null", i);
            return -EINVAL;
        }
        nodes[i]->path->progress = progress;
        if (0 != (ret = calcNextDuty(nodes[i]))) {
            pr("Error %d setting node %d: %s",
                    -ret, i, strerror(-ret));
            break;
        } else if (0 != (ret = setDuty(nodes[i]))) {
            pr("Error %d setting node %d: %s",
                    -ret, i, strerror(-ret));
            break;
        }
    }
    return ret;
}
#endif

int setDuty(tNode* node)
{
    if (node->duty > node->max_duty) node->duty = node->max_duty;
    if (node->duty < node->min_duty) node->duty = node->min_duty;
    struct servo_ioctl_pkt pkt;
    //pr("before duty = %d ", node->duty);
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = node->index;
    pkt.duty_ns = node->duty;
#ifndef DRY_RUN
    if (0 != (ioctl(fd, SERVO_IOC_SET_DUTY_NS, &pkt))) {
        pr("Error %d calling set ioctl: %s", errno, strerror(errno));
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

int setPath(
        tPath** pPath,
        int start_duty,
        int duty_goal,
        struct timespec *frameDelay)
{
    if (!path || !frameDelay) return -EINVAL;
    tPath *path = NULL;
    if (NULL == (path = malloc(sizeof(tPath)))) {
        pr( "couldn't allocate memory for path struct");
        return -ENOMEM;
    }

    path->start_duty = start_duty;
    path->target_duty = duty_goal;
    path->progress_step = fabs((float)DUTY_STEP / (float)(duty_goal - start_duty));

//    frameDelay->tv_nsec = 2222222;
#if 0
    if (abs(duty_start - duty_end) < 100) {
        return 0;
    }
    
    int duty_delta = duty_goal - duty_start;
    pr("duty_delta = duty_end (%d) - duty_start(%d) = %d", 
            duty_goal, duty_start, duty_delta);
    int num_steps = abs(duty_delta / 3999);
    pr("num_steps = abs(duty_delta / 3999) = %d", num_steps);
    pr("progress_step = duty_step / duty_delta = %f", progress_step);
    float duration = fabs((float)duty_delta / (float)1800);
    pr("duration = abs(duty_delta / 1800000) = %f", duration);
    float time_delta = (duration * 1E3);
    pr("time_delta = (duration * 1E9) = %d", time_delta);
    int time_step = time_delta  / num_steps;
    pr("time_step = time_delta  / num_steps = %d", time_step);
#endif
    //tPath path = { duty_start, duty_goal, 0, NULL };


    path->path_func = gentle2;

    *pPath = path;

    return 0;
}

float clock_delta(
        struct timespec t1,
        struct timespec t2)
{
    int d_s = t2.tv_sec - t1.tv_sec;
    int d_ns = t2.tv_nsec - t1.tv_nsec;
    return d_s ? d_s * 1E9 + d_ns : d_ns;
}
int sweep(tNode *node, int duty_end)
{
    int ret = 0;
    pr("duty_end = %d", duty_end);

    struct timespec delay = { 0 };

    if (0 != (ret = setPath(&node->path, node->duty, duty_end, &delay))) {
        pr( "error %d calculating params: %s",
                ret, strerror(-ret));
        return ret;
    }

    int step_count = 0;
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_REALTIME, &start_time);
    do {
        step_count++;
        node->path->progress += node->path->progress_step;
        calcNextDuty(node);
        if (0 != (ret = setDuty(node))) {
            pr( "Error %d setting duty %d for id %d: %s",
                    ret, node->duty, node->index, strerror(-ret));
        } else if (0 != (ret = servoSync(node))) {
            pr( "Error %d syncing for id %d: %s",
                    ret, node->index, strerror(-ret));
        } else if (0 != (ret = nanosleep(&delay, NULL))) {
            pr( "error %d calling nanosleep s=%d ns=%d: %s",
                    ret, delay.tv_sec, delay.tv_nsec, strerror(-ret));
        }

        //pr( "duty = %d goal = %d progress = %.2f",
        //        node->duty, node->path->target_duty, node->path->progress);
        assert(node->path->target_duty > 0);

        if (ret) break;
        if (node->path->progress < 0 || node->path->progress > 1)
        {
            break;
        }
    } while (abs(node->path->target_duty - node->duty) > 100);

    clock_gettime(CLOCK_REALTIME, &end_time);
    float duration = clock_delta(start_time, end_time);
    float step_duration = duration / step_count;
    pr("took %d steps in %.2f ms (%.2f ms/step)", 
            step_count, duration / 1E6, step_duration / 1E6);
    if (node->path) {
        free(node->path);
        node->path = NULL;
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
            pr("index out of range");
            return 0;
        }
    } else {
        pr("usage: %s <index 1-6> [<duty>]", argv[0]);
        return 0;
    }

    if (argc > 2) {
        duty_end = strtoul(argv[2], NULL, 10);
        setting = true;
    }


    fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        pr("Error %d opening %s: %s",
                fd, path, strerror(fd));
    } else {
        if (0 != (ret = getDuty(&g_node[index]))) {
            pr("getDuty returns %d: %s",
                    -ret, strerror(-ret));
        } else {
            if (setting) {
                sweep(&g_node[index], duty_end * g_node[index].a + g_node[index].b);
            }
            pr("duty: %d", (g_node[index].duty - g_node[index].b) / g_node[index].a);
        }
        close(fd);
    }
    return 0;
}
