#pragma once
/* distinct names to avoid incompatibility if <xpc/xpc.h> gets included somehow
 * on OS X.  No ARC support! */
#include <dispatch/dispatch.h>

typedef struct _xxpc_object {
    int x;
} *xxpc_object_t, *xxpc_connection_t, *xxpc_type_t;
typedef void (^xxpc_handler_t)(xxpc_object_t);

#define DEFINE_CONST(name, sym) \
    extern struct _xxpc_object name[1] asm("_" #sym)

DEFINE_CONST(XXPC_TYPE_CONNECTION, _xpc_type_connection);
DEFINE_CONST(XXPC_TYPE_ERROR, _xpc_type_error);
DEFINE_CONST(XXPC_TYPE_DICTIONARY, _xpc_type_dictionary);
DEFINE_CONST(XXPC_TYPE_ARRAY, _xpc_type_array);
DEFINE_CONST(XXPC_TYPE_STRING, _xpc_type_string);
DEFINE_CONST(XXPC_ERROR_CONNECTION_INTERRUPTED, _xpc_error_connection_interrupted);


#define XXPC_ARRAY_APPEND -1
#define XXPC_CONNECTION_MACH_SERVICE_LISTENER 1

#define WRAP(name, args...) \
    x##name args asm("_" #name)

void WRAP(xpc_release, (xxpc_object_t));
xxpc_object_t WRAP(xpc_retain, (xxpc_object_t));
char *WRAP(xpc_copy_description, (xxpc_object_t));

xxpc_type_t WRAP(xpc_get_type, (xxpc_object_t));
xxpc_object_t WRAP(xpc_string_create, (const char *));
void WRAP(xpc_array_append_value, (xxpc_object_t, xxpc_object_t));
xxpc_object_t WRAP(xpc_array_create, (const xxpc_object_t *, size_t));
size_t WRAP(xpc_array_get_count, (const xxpc_object_t));
xxpc_object_t WRAP(xpc_array_get_value, (xxpc_object_t, size_t));
const char *WRAP(xpc_array_get_string, (xxpc_object_t, size_t));
void WRAP(xpc_array_set_string, (xxpc_object_t, size_t, const char *));
xxpc_object_t WRAP(xpc_dictionary_create, (const char *const *,
                                           const xxpc_object_t *, size_t));

xxpc_object_t WRAP(xpc_dictionary_create_reply, (xxpc_object_t));
bool WRAP(xpc_dictionary_get_bool, (xxpc_object_t, const char *));
const char *WRAP(xpc_dictionary_get_string, (xxpc_object_t, const char *));
xxpc_object_t WRAP(xpc_dictionary_get_value, (xxpc_object_t, const char *));
void WRAP(xpc_dictionary_set_bool, (xxpc_object_t, const char *, bool));
void WRAP(xpc_dictionary_set_string, (xxpc_object_t, const char *, const char *));
void WRAP(xpc_dictionary_set_value, (xxpc_object_t, const char *, xxpc_object_t));

xxpc_connection_t WRAP(xpc_connection_create_mach_service, (const char *,
                                                            dispatch_queue_t,
                                                            uint64_t));
void WRAP(xpc_connection_resume, (xxpc_connection_t));
void WRAP(xpc_connection_set_event_handler, (xxpc_connection_t, xxpc_handler_t));
void WRAP(xpc_connection_send_message_with_reply,
    (xxpc_connection_t, xxpc_object_t, dispatch_queue_t, xxpc_handler_t));
void WRAP(xpc_connection_send_message, (xxpc_connection_t, xxpc_object_t));
void WRAP(xpc_connection_cancel, (xxpc_connection_t));

#undef DEFINE_TYPE
#undef DEFINE_CONST
#undef WRAP
