#include <spawn.h>
#include <stdlib.h>
#include <stdio.h>
int main(__attribute__((unused)) int argc, char **argv) {
    pid_t pid = 0;
    printf("doing it:\n");
    int ret = posix_spawn(&pid, argv[1], NULL, NULL, argv + 1, NULL);
    printf("==> %d pid=%d\n", ret, pid);
}
