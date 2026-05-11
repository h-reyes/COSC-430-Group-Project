#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(){
int flag = 1;
    while (flag){
        printf("oye estamos vivos \n");

        if(flag == 1){
            printf("printing \n");
            sleep(1);
        }
    }
}