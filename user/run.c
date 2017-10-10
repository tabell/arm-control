#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define PATH_ROOT "/sys/devices/platform/"

#define prerr(code, action) printf("Error %d %s: %s\n", code, action, strerror(code))

typedef struct node {
    int fd;
    uint8_t angle;
	const char* name;
} tNode;

const char *paths[] = {
   "base",
   "shoulder",
   "elbow",
   "wrist1",
   "wrist2",
    NULL };

int nodeCreate(const char* path, tNode** node)
{
    if (!path || !node) return -EINVAL;

    tNode *newnode = calloc(1, sizeof(tNode));
    if (!newnode) return -ENOMEM;

    int ret = 0;

    char sysPath[PATH_MAX];
    snprintf(sysPath, PATH_MAX, "%s%s/%s",
            PATH_ROOT, path, "angle");
    newnode->fd = open(sysPath, O_RDWR);
    if (-1 == newnode->fd) {
        printf("Error %d opening %s: %s\n",
            errno, sysPath, strerror(errno));
        ret = errno;
        free(newnode);
    } else {
        printf("opened new fd %d for %s\n", newnode->fd, sysPath);
        newnode->name = path;
        *node = newnode;
    }


    return ret;
}

int nodeGet(tNode* node, int *angle)
{
    if (!node || node->fd < 0) return -EINVAL;

    char strAngle[32];
    ssize_t nRead;
    int ret = 0;
    int newAngle;


    if (0 > (nRead = read(node->fd, strAngle, sizeof(strAngle)))) {
        printf("Error %d reading from %s: %s\n", errno, node->name, strerror(errno));
        ret = errno;
    } else if (0 > (newAngle = strtoul(strAngle, NULL, 10))) {
        printf("Error %d converting %s: %s\n", newAngle, strAngle, strerror(-newAngle));
        ret = newAngle;
    } else {
        *angle = newAngle;
    }

    return ret;

}
int nodeSet(tNode* node, int angle)
{
    if (!node || node->fd < 0) return -EINVAL;

    char strAngle[32];
    ssize_t nWritten;
    int ret = 0;

    if (0 > (snprintf(strAngle, 32, "%d", angle))) {
        printf("sprintf error %d: %s\n", errno, strerror(errno));
    } else {
        if (0 > lseek(node->fd, 0, SEEK_SET)) {
            printf("error %d seeking fd %d: %s\n",
                    errno, node->fd, strerror(errno));
        } else if (0 > (nWritten = write(node->fd, strAngle, ret))) {
            printf("error %d writing %s to fd %d: %s\n",
                    errno, strAngle, node->fd, strerror(errno));
            ret = -errno;
        } else {
            node->angle = angle;
        }
    }

    return ret;
}

int nodeDestroy(tNode **node)
{
    if (!node) return -EINVAL;

    close((*node)->fd);

    if (*node) free(*node);
    *node = NULL;

    return 0;
}

int main(int argc, char *argv[])
{
    int i = 0;
    int ret;
    int angle;
    tNode* node[5];

    for (i = 0; i < 5; i++)
    {
        node[i] = NULL;
        if (0 != (ret = nodeCreate(paths[i], &node[i]))) {
            printf("Error %d creating node %i: %s\n", ret, i, strerror(-ret));
        }
    }
    for (i = 0; i < 5; i++)
    {
        if (0 != (ret = (nodeGet(node[i], &angle)))) {
            printf("Error %d getting angle from node %i: %s\n", ret, i, strerror(-ret));
        } else if (0 != (ret = (nodeSet(node[i], angle)))) {
            printf("Error %d setting angle for node %i: %s\n", ret, i, strerror(-ret));
        } else if (0 != (ret = (nodeGet(node[i], &angle)))) {
            printf("Error %d getting angle from node %i: %s\n", ret, i, strerror(-ret));
        } else {
            if (0 != (ret = (nodeGet(node[i], &angle)))) {
                printf("Error %d getting angle from node %i: %s\n", ret, i, strerror(-ret));
            } else if (0 != (ret = (nodeSet(node[i], angle)))) {
                printf("Error %d setting angle for node %i: %s\n", ret, i, strerror(-ret));
            } else printf("%s : %d\n", paths[i], angle);
            nodeDestroy(&node[i]);
            printf("%s : %d\n", paths[i], angle);
        }

        nodeDestroy(&node[i]);
    }

    return 0;
}
