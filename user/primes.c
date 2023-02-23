#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p[2][2];
    int index = 0;
    pipe(p[index]);
    int min = 2;
    for (int i = min; i <= 35; ++i)
    {
        write(p[index][1], &i, sizeof(int));
    }
    close(p[index][1]);

    while (fork() == 0)
    {
        if (read(p[index][0], &min, sizeof(int)) > 0)
        {
            fprintf(1, "prime %d\n", min);
        }
        else
        {
            exit(0);
        }
        pipe(p[index ^ 1]);
        int buf;
        while (read(p[index][0], &buf, sizeof(int)) > 0)
        {
            if (buf % min)
            {
                write(p[index ^ 1][1], &buf, sizeof(int));
            }
        }
        close(p[index][0]);
        close(p[index ^ 1][1]);
        index ^= 1;
    }
    close(p[index][0]);
    wait((void *)0);
    exit(0);
}