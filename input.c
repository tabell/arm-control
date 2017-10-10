#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <poll.h>
#include <termios.h>

#include "pwm-servo.h"

struct servo_state state[6];

int getState(
        int fd,
        struct servo_state state[6])
{
    if (!state) {
        return -EINVAL;
    }

    int ret = ioctl(fd, SERVO_IOC_GET_STATE, state);

    if (ret != 0) {
        fprintf(stderr, "ioctl returns %d: %s\n", errno, strerror(errno));
    }
    return ret;
}
int setState(
        int fd,
        struct servo_state *state)
{
    if (!state) {
        return -EINVAL;
    }

    int ret = ioctl(fd, SERVO_IOC_SET_STATE, state);

    if (ret != 0) {
        fprintf(stderr, "ioctl returns %d: %s\n", errno, strerror(errno));
    }
}

void enable(
        struct servo_state *state,
        int index)
{
    if (!state) return -EINVAL;

    state[index].enabled = 1;

}

void disable(
        struct servo_state *state,
        int index)
{
    if (!state) return -EINVAL;

    state[index].enabled = 0;
}

int disableAll(
        int fd,
        struct servo_state *state)
{
    int i;
    if (!state) return -EINVAL;
    for (i = 0; i < 5; i++)
    {
        state[i].enabled = 0;
    }
    return setState(fd, state);
}

void printState(
        struct servo_state state[6])
{
    if (!state) {
        return;
    }

    fprintf(stderr, "base %d, shoulder %d, elbow %d, wrist1 %d, wrist2 %d, claw %d\n",
            state[0].angle,
            state[1].angle,
            state[2].angle,
            state[3].angle,
            state[4].angle,
            state[5].angle);
}


int main() {
    const char *path = "/dev/pwm-servo";
    bool running = true;
    struct pollfd pf[1];
    char inbuf[4096] = {0};
    ssize_t nread;
    int i;
    struct termios term, oldterm;

    fprintf(stderr, "trying to open %s\n", path);
    int fd = open(path,
            O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening %s: %s\n",
                fd, path, strerror(fd));
    } else {
        getState(fd, state);

        int x;

        tcgetattr(fileno(stdin), &term);
        tcgetattr(fileno(stdin), &oldterm);
        cfmakeraw(&term);
        tcsetattr(fileno(stdin), 0, &term);


        state[0].enabled = true;
        state[0].angle = 90;
        setState(fd, state);

        while (running) {
            usleep(1000);
            pf[0].fd  =  fileno(stdin);
            pf[0].events = POLLIN;
            poll(pf, 1, 0);
            if (pf[0].revents & POLLIN) {
                nread = read(fileno(stdin), inbuf, 4096);
                if (nread > 0) {
                    for (i = 0; i < nread; i++) {
                        switch(inbuf[i]) {
                            case 3:
                            case 27:
                                running = false;
                                tcsetattr(fileno(stdin), 0, &oldterm);

                                
                                
                                break;
                            case 'q':
                                state[0].angle += 1;
                                break;
                            case 'a':
                                state[0].angle -= 1;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }

            setState(fd, state);
        }

        disableAll(fd, state);
        setState(fd, state);

        printState(state);

        close(fd);
    }

    return 0;
}
