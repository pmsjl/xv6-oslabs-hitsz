#include "kernel/types.h"
#include "user/user.h"
int 
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    int father_pid=getpid();
    char parent[1];
    char child[1];
    char b = 'p';
    pipe(p1);
    pipe(p2);
    int pid = fork();
    if (pid == 0)
    {
        read(p1[0], child, 1);
        close(p1[0]);

        fprintf(1, "%d: received ping from pid %d\n",getpid(),father_pid);
        write(p2[1], child, 1);
        close(p2[1]);
        exit(0);
    }
    else
    {
        write(p1[1], &b, 1);
        close(p1[1]);

        read(p2[0], parent, 1);
        fprintf(1, "%d: received pong from pid %d\n", father_pid,pid);
        close(p2[0]);

        wait(0);
        exit(0);
    }
}
// 往往双向通信采取的是一对管道，两个管道，然后一个发一个收，不是一个管道混着用
// 我再总结一下我犯的错误和对应的知识点，read和write都需要等待，read只有当写入端被close掉才会关闭，否则会一直维持这个状态，而write则需要有另一端read到数据才会关闭否则也会一直维持该状态，所以不能再read前面加wait，然后wait是父进程等待子进程关闭的，一般放在父进程退出前，防止子进程成为孤儿进程无法正常退出