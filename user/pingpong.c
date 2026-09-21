#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    int father_pid = getpid();
    char parent[1];
    char child[1];
    char b = 'p';

    pipe(p1);
    pipe(p2);

    int pid = fork();
    if (pid == 0)
    {
        close(p1[1]);
        close(p2[0]);

        read(p1[0], child, 1);
        close(p1[0]);

        fprintf(1, "%d: received ping from pid %d\n", getpid(), father_pid);
        write(p2[1], child, 1);
        close(p2[1]);
        exit(0);
    }
    else
    {
        close(p1[0]);
        close(p2[1]);

        write(p1[1], &b, 1);
        close(p1[1]);

        read(p2[0], parent, 1);
        fprintf(1, "%d: received pong from pid %d\n", father_pid, pid);
        close(p2[0]);

        wait(0);
        exit(0);
    }
}
