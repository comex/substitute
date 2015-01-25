#include "substitute-internal.h"
#include "stop-other-threads.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
static void *some_thread(void *ip) {
    long i = (long) ip;
    while (1) {
        printf("Hello from %ld\n", i);
        sleep(1);
    }
}
static void replacement() {
    printf("Bye\n");
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
    assert(!stop_other_threads(&stop_token));
    assert(!apply_pc_patch_callback(stop_token, patch_callback, NULL));
    assert(!resume_other_threads(stop_token));
    void *out;
    for (long i = 0; i < 10; i++)
        assert(!pthread_join(pts[i], &out));
}
