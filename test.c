#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define PATH_ROOT "/sys/devices/platform/"

#define prerr(code, action) printf("Error %d %s: %s\n", code, action, strerror(code))

typedef struct node {
    int fd;
    uint8_t angle;
	const char* path;
} tNode;

const char *paths[] = { "base",
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

    newnode->path = path;

    *node = newnode;

    return 0;
}

int nodeGet(tNode* node, int *angle)
{
    if (!node || node->fd < 0) return -EINVAL;

    char strAngle[32];
    ssize_t nRead;
    int ret = 0;
    int newAngle;

    node->fd = open(node->path, O_APPEND);
    if (-1 == node->fd) {
        printf("Error %d opening %s: %s\n",
            node->fd, node->path, strerror(node->fd));
    } else if (0 > (nRead = read(node->fd, strAngle, sizeof(strAngle)))) {
        printf("Error %d reading from %s: %s\n", newAngle, node->path, strerror(-newAngle));
    } else if (0 > (newAngle = strtoul(strAngle, NULL, 10))) {
        printf("Error %d converting %s: %s\n", newAngle, strAngle, strerror(-newAngle));
        ret = newAngle;
    } else {
        *angle = newAngle;
    }
    close(node->fd);

    return ret;

}
int nodeSet(tNode* node, int angle)
{
    if (!node || node->fd < 0) return -EINVAL;

    char strAngle[32];
    ssize_t nWritten;
    int ret = 0;

    node->fd = open(node->path, O_APPEND);
    if (-1 == node->fd) {
        printf("Error %d opening %s: %s\n",
            node->fd, node->path, strerror(node->fd));
    } else {
        if (0 > (ret = snprintf(strAngle, 32, "%d", angle))) {
            printf("sprintf error %d: %s\n", ret, strerror(-ret));
        } else {
            nWritten = write(node->fd, strAngle, ret);
            node->angle = angle;
        }
        close(node->fd);
    }

    return ret;
}

int nodeDestroy(tNode **node)
{
    if (!node) return -EINVAL;

    free(*node);
    *node = NULL;

    return 0;
}

int main(int argc, char *argv[])
{
    int i = 0;
    int ret;
    int angle;
    tNode* node[5];

    while (paths[i] != NULL)
    {

        if (0 != (ret = nodeCreate(paths[i], &node[i]))) {
            printf("Error %d creating node %i: %s\n", ret, i, strerror(-ret));
        } else if (0 != (ret = (nodeGet(node[i], &angle)))) {
            printf("Error %d getting angle from node %i: %s\n", ret, i, strerror(-ret));
        } else if (0 > printf("%s : %d\n", paths[i], angle)) {
        } else {
            nodeDestroy(&node[i]);
        }
    }

    return 0;
}

