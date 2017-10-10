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


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: %s <index 1-6> <command: set/get/on/off> [<duty cycle ns>]\n", argv[0]);
        return 0;
    }
    const char *path = "/dev/robot";

    fprintf(stderr, "trying to open %s\n", path);
    int fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening %s: %s\n",
                fd, path, strerror(fd));
    } else {

        fprintf(stderr,"opened fd %d for %s\n",
                fd, path);

        //int angle[5] = { -1, -1, -1, -1, -1 };
#if 0
        int ret = ioctl(fd, SERVO_IOC_GET_STATE, &state);

        if (ret != 0) {
            fprintf(stderr, "ioctl returns %d: %s\n", errno, strerror(errno));
        } else {
            fprintf(stderr, "base %d, shoulder %d, elbow %d, wrist1 %d, wrist2 %d, claw %d\n",
                    state[0].enabled ? state[0].angle : -1,
                    state[1].enabled ? state[1].angle : -1,
                    state[2].enabled ? state[2].angle : -1,
                    state[3].enabled ? state[3].angle : -1,
                    state[4].enabled ? state[4].angle : -1,
                    state[5].enabled ? state[5].angle : -1);
        }


        state[2].enabled = 1;
        state[3].enabled = 1;
        state[4].enabled = 1;
        state[5].enabled = 1;

        ret = ioctl(fd, SERVO_IOC_SET_STATE, &state);

        if (ret != 0) {
            fprintf(stderr, "ioctl returns %d: %s\n", errno, strerror(errno));
        } else {
            fprintf(stderr, "base %d, shoulder %d, elbow %d, wrist1 %d, wrist2 %d, claw %d\n",
                    state[0].enabled ? state[0].angle : -1,
                    state[1].enabled ? state[1].angle : -1,
                    state[2].enabled ? state[2].angle : -1,
                    state[3].enabled ? state[3].angle : -1,
                    state[4].enabled ? state[4].angle : -1,
                    state[5].enabled ? state[5].angle : -1);
        }
#endif

        close(fd);
    }

    return 0;
}
