#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "pwm-servo.h"


int main() {
    const char *path = "/dev/pwm-servo";
    fprintf(stderr, "trying to open %s\n", path);
    int fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE|O_CLOEXEC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening %s: %s\n",
                fd, path, strerror(fd));
    } else {
        fprintf(stderr,"opened fd %d for %s\n",
                fd, path);

        int angle = -1;
        //int angle[5] = { -1, -1, -1, -1, -1 };

        int ret = ioctl(fd, SERVO_IOC_GET_ANGLE, &angle);

        if (ret != 0) {
            fprintf(stderr, "ioctl returns %d: %s\n", errno, strerror(errno));
        } else {
            fprintf(stderr, "success: angle = %d\n", angle);
        }


        close(fd);
    }
    return 0;
}
