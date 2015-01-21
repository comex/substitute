#include "substitute.h"
#include "substitute-internal.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc <= 1) {
        printf("usage: test-inject <pid>\n");
        return 1;
    }
    int pid = atoi(argv[1]);
    char *error = NULL;
    int ret = substitute_dlopen_in_pid(pid, "/tmp/hello", 0, &error);
    printf("ret=%d err=%s\n", ret, error);
}
