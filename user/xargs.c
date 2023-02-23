#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void readline(char *buf, char *lines[MAXARG], int start)
{
    char *cur_line = buf;
    while (cur_line)
    {
        char *next_line = strchr(cur_line, '\n');
        if (next_line)
        {
            *next_line = '\0';
            lines[start] = cur_line;
            ++start;
            cur_line = next_line + 1;
        }
        else
        {
            lines[start] = cur_line;
            ++start;
            cur_line = 0;
        }
    }
}
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "args ...\n");
        exit(1);
    }
    char buf[128];
    read(0, buf, sizeof(buf));
    if (fork() == 0)
    {
        char *lines[MAXARG];
        lines[0] = argv[1];
        int i;
        for (i = 2; i < argc; ++i)
        {
            lines[i - 1] = argv[i];
        }
        readline(buf, lines, i - 1);
        exec(argv[1], lines);
        exit(0);
    }
    wait(0);
    exit(0);
}