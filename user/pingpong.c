#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    if (fork() == 0)
    {
        char buf[5];
        read(p[0], &buf, sizeof(int));
        printf("%d: received %s\n", getpid(), buf);
        write(p[1], "pong", 5);
        close(p[0]);
        close(p[1]);
    }
    else
    {
        write(p[1], "ping", 4);
        wait(0);
        char buf[5];
        read(p[0], buf, sizeof(buf));
        printf("%d: received %s\n", getpid(), buf);
        close(p[0]);
        close(p[1]);
    }
    exit(0);
}