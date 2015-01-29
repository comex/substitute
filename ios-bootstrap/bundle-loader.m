#include <syslog.h>
__attribute__((constructor))
static void init() {
    syslog(LOG_WARNING, "Hi!");
}
