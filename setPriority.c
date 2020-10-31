#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[]){
    printf(1, "in setPriority\n");
    if (argc != 3){
        printf(2, "setPriority: wrong number of arguments\n");
        exit();
    }
    int nPri = atoi(argv[1]), pid = atoi(argv[2]);
    printf(1, "%d %d\n", nPri, pid);
    set_priority(nPri, pid);
    exit(); 
}
