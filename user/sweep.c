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

#include "../kernel/servo.h"

#define DEF_DUTY 900000

#define pr(fmt, ...) fprintf(stderr, "<%s:%d> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#define DRY_RUN
//#define DEBUG_PRINT

#define TIMESPEC_COPY(dest,src) do { \
    dest.tv_sec = src.tv_sec; \
    dest.tv_nsec = src.tv_nsec; } while(0);

#define SPEED_NORMAL 524288

int fd = -1;
const char *path = "/dev/robot";

typedef float (*tPathFunc)(float);

typedef struct path {
    int start_duty;
    int target_duty;
    float progress;
    float last_progress;
    float progress_unit;
    int num_steps;
    bool done;
    tPathFunc path_func; 
} tPath;

typedef struct node {
    int index;
    int min_duty;
    int max_duty;
    int duty;
    int duty_default;
    int last_duty;
    int a;
    int b;
    tPath *path;
} tNode;

tNode g_node[] = {{   .index = 0, .min_duty = 600000,  .max_duty = 2400000, .duty = 0, .duty_default = 600000, .a = 10000, .b = 0 },
                    { .index = 1, .min_duty = 600000,  .max_duty = 2600000, .duty = 0, .duty_default = 1700000, .a = 10000, .b = 0 },
                    { .index = 2, .min_duty = 600000,  .max_duty = 2400000, .duty = 0, .duty_default = 800000, .a = 10000, .b = 0 },
                    { .index = 3, .min_duty = 600000,  .max_duty = 2400000, .duty = 0, .duty_default = 2000000, .a = 10000, .b = 0 },
                    { .index = 4, .min_duty = 600000,  .max_duty = 2400000, .duty = 0, .duty_default = 1400000, .a = 10000, .b = 0 },
                    { .index = 5, .min_duty = 1800000, .max_duty = 2400000, .duty = 0, .duty_default = 1800000, .a = 10000, .b = 0 }};


typedef enum command {
    COMMAND_UNKNOWN,
    CMD_ON,
    CMD_OFF,
    CMD_SET,
    CMD_GET,
    CMD_SYNC,
} tCmd;

/* --------------------------------------------------*/
/* Foward declarations */
/* --------------------------------------------------*/
int getDuty(tNode* node);

/* --------------------------------------------------*/
/* Function definitions */
/* --------------------------------------------------*/
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

int initNode(tNode* node)
{
    if (!node) return -EINVAL;

    int ret = 0;

    if (0 != (ret = getDuty(node))) {
        pr("Error %d getting duty: %s", ret, strerror(-ret));
    } else if (node->duty == 0) {
        node->duty = node->duty_default;
    }

    return ret;
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
    if (abs(node->path->target_duty - node->duty) > 100) {
        free(node->path);
        node->path = NULL;
    }
    return 0;
}

int setDuty(tNode* node)
{
    if (node->duty > node->max_duty) node->duty = node->max_duty;
    if (node->duty < node->min_duty) node->duty = node->min_duty;
    struct servo_ioctl_pkt pkt;
    pr("node %d: duty = %d ", node->index, node->duty);
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
        int duty_goal)
{
    if (!path) return -EINVAL;
    tPath *path = NULL;
    if (NULL == (path = malloc(sizeof(tPath)))) {
        pr( "couldn't allocate memory for path struct");
        return -ENOMEM;
    }

    path->start_duty = start_duty;
    path->target_duty = duty_goal;

    float duration = fabs((float)(duty_goal - start_duty)) * 1600;
    path->progress_unit = 1.0f / duration;

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

int getMaxDelta(
        tNode* nodes[6],
        int* pMaxDelta)
{
    if (!nodes) return -EINVAL;

    int max_delta = 0;

    for (int i = 0; i < 6; i++)
    {
        if (!nodes[i]->path) continue;

        int delta = (nodes[i]->path->target_duty - nodes[i]->path->start_duty);

        if (delta > max_delta) max_delta = delta;

    }
    *pMaxDelta = max_delta;
    return 0;
}

int multiSweep(tNode *nodes[6], int duty_end[6])
{
    int ret = 0;
    for (int n = 0; n < 5; n++) {
        tNode *node = nodes[n];
        if (!node) continue;

        if (node->duty == 0) {
            node->duty = node->duty_default;
        }
        pr("%d: duty_start = %d duty_end = %d",
                n, node->duty, duty_end[n]);

        if (0 != (ret = setPath(&node->path, node->duty, duty_end[n]))) {
            pr( "error %d calculating params: %s",
                    ret, strerror(-ret));
            return ret;
        }
    }
    

    int step_count = 0;
    struct timespec start_time, end_time, last;
    clock_gettime(CLOCK_REALTIME, &start_time);

    TIMESPEC_COPY(last, start_time);
    
    do {
        /* Loop Sync */
        clock_gettime(CLOCK_REALTIME, &end_time);
        float tick = clock_delta(last, end_time);
        //pr("%d: %.2f us: progress = %.2f", step_count, tick / 1E3, node->path->progress);
        TIMESPEC_COPY(last, end_time);
        step_count++;

        for (int n = 0; n < 5; n++) 
        {
            /* per node */
            tNode *node = nodes[n];
            if (!node || !node->path) continue;

            /* Update progress for this node */
            node->path->progress += node->path->progress_unit * tick;

            /* Apply new duty to node and update kernel */
            /* TODO: Should not need to context switch once per node
             * but that's how the driver is currently written */
            if (0 != (ret = setDuty(node))) {
                pr( "Error %d setting duty %d for id %d: %s",
                        ret, node->duty, node->index, strerror(-ret));
                break;
            } else if (0 != (ret = servoSync(node))) {
                pr( "Error %d syncing for id %d: %s",
                        ret, node->index, strerror(-ret));
                break;
            }

            /* Debug tracking */
            node->last_duty = node->duty;
            node->path->last_progress = node->path->progress;
#if 0

             pr( "duty = %d goal = %d progress = %.5f",
                      node->duty, node->path->target_duty, node->path->progress);
             assert(node->path->target_duty > 0);

             pr( "diff duty = %d, progress = %.5f, duty/time = %f",
                     node->duty - node->last_duty,
                     node->path->progress - node->path->last_progress,
                     (node->duty - node->last_duty) *1E6 / tick);

#endif

            /* Calculate new duty based on progress and path function */
            calcNextDuty(node);
        }

    } while (nodes[0]->path || nodes[1]->path ||
                nodes[2]->path || nodes[3]->path ||
                nodes[4]->path || nodes[5]->path);
    //} while (abs(node->path->target_duty - node->duty) > 100);

    clock_gettime(CLOCK_REALTIME, &end_time);
    float duration = clock_delta(start_time, end_time);
    float step_duration = duration / step_count;
    int maxDelta = 0;
    getMaxDelta(nodes, &maxDelta);
    float ns_per_duty = duration / maxDelta;
    pr("took %d steps in %.2f ms (%.2f ms/step), ns_per_duty = %f", 
            step_count, duration / 1E6, step_duration / 1E6, ns_per_duty);

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

    int duty_goals[6] = { 60 * 1E5, 110 * 1E5, 80 * 1E5, 200 * 1E5, 40 *1E5, 180 *1E5};


    fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        pr("Error %d opening %s: %s",
                errno, path, strerror(errno));
    } else {
        for (int i = 0; i < 5; i++) {
            if (0 != (ret = initNode(&g_node[i]))) {
                pr("initNode returns %d: %s",
                        -ret, strerror(-ret));
                break;
            }
        }
        if (ret == 0) {
        //    pr("duty: %d", (g_node[index].duty - g_node[index].b) / g_node[index].a);
            if (setting) {
                tNode *nodes[] = { &g_node[0], &g_node[1], &g_node[2], &g_node[3], &g_node[4], &g_node[5] };
                multiSweep(&nodes, &duty_end);
            }
        //    pr("duty: %d", (g_node[index].duty - g_node[index].b) / g_node[index].a);
        }
        close(fd);
    }
    return 0;
}
