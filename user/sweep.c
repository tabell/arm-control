#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "servo.h"

int fd = -1;

typedef enum command {
    COMMAND_UNKNOWN,
    CMD_ON,
    CMD_OFF,
    CMD_SET,
    CMD_GET,
    CMD_SYNC,
} tCmd;

int setDuty(int index, int duty)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = index;
    pkt.duty_ns = duty;

    if (0 != (ioctl(fd, SERVO_IOC_SET_DUTY_NS, &pkt))) {
        return -errno;
    }
    return 0;
}

int getDuty(int index, int* duty)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = index;

    if (0 != (ioctl(fd, SERVO_IOC_GET_DUTY_NS, &pkt))) {
        return -errno;
    }
    *duty = pkt.duty_ns;
    return 0;

}

int servoSync(int index)
{
    struct servo_ioctl_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.idx = index;

    if (0 != (ioctl(fd, SERVO_IOC_SYNC, &pkt))) {
        return -errno;
    }
    return 0;


}
int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <index 1-6> <min duty> <max duty> <time>\n", argv[0]);
        return 0;
    }

    /* Allocation */
    int ret;

    /* parse index */
    int index = strtoul(argv[1], NULL, 10);
    if ((index > 5) || (index < 0)) {
        fprintf(stderr,"index out of range\n");
        return 0;
    }

    int duty_start = strtoul(argv[2], NULL, 10);
    int duty_end = strtoul(argv[3], NULL, 10);

    float duration = strtof(argv[4], NULL);

    fprintf(stderr, "sweeping node %d from %d to %d in %f s\n",
            index, duty_start, duty_end, duration);
    
    int num_steps = duration * 100;
    fprintf(stderr, "num_steps = %d\n", num_steps);

    int duty_delta = duty_end - duty_start;
    int duty_step = duty_delta / num_steps;
    fprintf(stderr, "duty: delta = %d step = %d\n", duty_delta, duty_step);
    float time_delta = (duration * 1E9);
    int time_step = time_delta  / num_steps;
    fprintf(stderr, "time: delta = %f ns step = %d ns\n", time_delta, time_step);

    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = abs(time_step),
    };

    const char *path = "/dev/robot";

    fprintf(stderr, "trying to open %s\n", path);
    fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening %s: %s\n",
                fd, path, strerror(fd));
    } else {

        fprintf(stderr,"opened fd %d for %s\n",
                fd, path);
        int duty = duty_start;
        fprintf(stderr, "start = %d goal = %d step = %d\n",
                duty, duty_end, duty_step);

        fprintf(stderr, "abs(duty - duty_end) = %d, abs(duty_step) = %d\n", abs(duty - duty_end), abs(duty_step));
        for (duty = duty_start; abs(duty - duty_end) > abs(duty_step); duty += duty_step) {
            fprintf(stderr, "start = %d current = %d end = %d step = %d\n",
                    duty_start, duty, duty_end, duty_step);
            if (0 != (ret = setDuty(index, duty))) {
                fprintf(stderr, "Error %d setting duty %d for id %d: %s\n",
                        ret, duty, index, strerror(-ret));
            } else if (0 != (ret = servoSync(index))) {
                fprintf(stderr, "Error %d syncing for id %d: %s\n",
                        ret, index, strerror(-ret));
            } else if (0 != (ret = nanosleep(&delay, NULL))) {
                fprintf(stderr, "error %d calling nanosleep: %s\n",
                        ret, strerror(-ret));
            }

            if (!ret) break;
        }



        close(fd);
    }

    fprintf(stderr, "clean exit\n");
    return 0;
}
