#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


int main(int argc, char *argv[])
{

    if (argc < 1){
        printf(2, "time: number of arguments is less than 1\n");
        exit();
    }
    //----debugg
    for (int j = 0; j < argc; j++)
    {
        printf(1, "%s   ", argv[j]);
    }
    printf(1, "\n");
    //----

    int pid = fork();

    if (pid < 0)
    {
        printf(2, "time: fork failed\n");
        exit();
    }

    if (pid == 0){
        int exRet = exec(argv[1], argv + 1);
        if (exRet < 0){
            //child proc failed ;-;
            printf(2, "time: child process failed\n");
        }
        exit();
    }

    if (pid > 0){
        int waitTime = 0, runTime = 0, wxRet = 0;
        wxRet =waitx(&waitTime, &runTime);
        printf(1, "time: status: %d process %d, had wait time %d, runtine %d\n", wxRet, pid, waitTime, runTime );

        exit();
    }

    exit();
}
