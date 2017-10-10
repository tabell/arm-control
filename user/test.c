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
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s <index 1-6> <command: set/get/on/off> [<duty cycle ns>]\n", argv[0]);
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

    tCmd cmd;

    /* parse command */
    if (!strcmp(argv[2], "sync")) {
        cmd = CMD_SYNC;
        /* turn on */
    } else if (!strcmp(argv[2], "on")) {
        cmd = CMD_ON;
        /* turn on */
    } else if (!strcmp(argv[2], "off")) {
        /* turn off */
        cmd = CMD_OFF;
    } else if (!strcmp(argv[2], "get")) {
        /* get duty */
        cmd = CMD_GET;

    } else if (argc < 4) {
        fprintf(stderr,"that command requires another parameter\n");
        return 0;
    } else if (!strcmp(argv[2], "set")) {
        /* set duty */
        cmd = CMD_SET;
    } else {
        fprintf(stderr,"Unrecognized command\n");
        return 0;
    }


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

        if (cmd == CMD_GET) {
            int duty = -1;
            if (0 != (ret = getDuty(index, &duty))) {
                fprintf(stderr, "Error %d getting duty for id %d: %s\n",
                        ret, index, strerror(-ret));
            } else {
                fprintf(stderr,"id %d returns duty=%d ns\n", index, duty);
            }
        } else if (cmd == CMD_SET) {
            int duty = strtoul(argv[3], NULL, 10);
            fprintf(stderr, "setting index %d, duty %d\n",
                    index, duty);
            if (0 != (ret = setDuty(index, duty))) {
                fprintf(stderr, "Error %d setting duty %d for id %d: %s\n",
                        ret, duty, index, strerror(-ret));
            } else {
                fprintf(stderr,"id %d set duty=%d ns\n", index, duty);
            }
        } else if (cmd == CMD_SYNC) {
            fprintf(stderr, "calling sync index %d\n",
                    index);
            if (0 != (ret = servoSync(index))) {
                fprintf(stderr, "Error %d syncing for id %d: %s\n",
                        ret, index, strerror(-ret));
            }
        }



        close(fd);
    }

    return 0;
}
