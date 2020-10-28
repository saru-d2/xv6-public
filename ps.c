#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

int main(int argc, char *argv[]){
    printf(1, "ps\n");
    if (argc <= 0){
        printf(2, "ps: argc <= 0\n");
        exit();
    }
    if (argc >= 2){
        printf(2, "ps: too many arguments, correct usage is no arguments\n");
        exit();
    }
    psx(); 
    exit();   
}