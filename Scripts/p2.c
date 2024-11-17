// wait() system call

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h> 

int main(int argc, char* argv[]) {

    int id = fork();
    int n;  // number that we start at

    if (id == 0) {
        // child process
        n = 1;
    } else {
        // parent or main process
        n = 6;
    }

    if (id != 0) {
        wait(NULL);
    }

    int i;
    for (i = n; i < n + 5; i++) {
        printf("%d ", i);
        fflush(stdout);
    }
    if (id != 0) {
        printf("\n");
    }
    return 0;
}