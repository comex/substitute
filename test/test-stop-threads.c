#include "substitute-internal.h"
#include "stop-other-threads.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
/* printf without taking any locks - because they might be taken at stop time */
#define ulprintf(...) do { \
    char buf[256]; \
    int len = sprintf(buf, __VA_ARGS__); \
    write(1, buf, len); \
} while(0)

static void *some_thread(void *ip) {
    long i = (long) ip;
    while (1) {
        ulprintf("Hello from %ld\n", i);
        sleep(1);
    }
}
static void replacement() {
    ulprintf("Bye\n");
    pthread_exit(NULL);
}
static uintptr_t patch_callback(void *ctx, UNUSED uintptr_t pc) {
    assert(!ctx);
    return (uintptr_t) replacement;
}

int main() {
    pthread_t pts[10];
    for (long i = 0; i < 10; i++)
        pthread_create(&pts[i], NULL, some_thread, (void *) i);
    sleep(1);
    void *stop_token;
    ulprintf("stopping\n");
    assert(!stop_other_threads(&stop_token));
    ulprintf("stopped\n");
    assert(!apply_pc_patch_callback(stop_token, patch_callback, NULL));
    ulprintf("resuming\n");
    assert(!resume_other_threads(stop_token));
    ulprintf("resumed\n");
    void *out;
    for (long i = 0; i < 10; i++)
        assert(!pthread_join(pts[i], &out));
}
