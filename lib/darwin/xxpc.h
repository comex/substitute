#pragma once
/* distinct names to avoid incompatibility if <xpc/xpc.h> gets included somehow
 * on OS X. */
#include <dispatch/dispatch.h>
#include <os/object.h>
#include <mach/message.h> /* for audit_token_t */

#if OS_OBJECT_USE_OBJC
#define DC_CAST (__bridge xxpc_object_t)
OS_OBJECT_DECL(xxpc_object);
#else
#define DC_CAST
typedef struct xxpc_object *xxpc_object_t;
#endif
typedef xxpc_object_t xxpc_connection_t, xxpc_type_t;
typedef void (^xxpc_handler_t)(xxpc_object_t);

#define DEFINE_CONST(name, sym) \
   extern struct xxpc_object x_##name asm("_" #sym); \
   static const xxpc_object_t name = DC_CAST &x_##name

DEFINE_CONST(XXPC_TYPE_CONNECTION, _xpc_type_connection);
DEFINE_CONST(XXPC_TYPE_BOOL, _xpc_type_error);
DEFINE_CONST(XXPC_TYPE_ERROR, _xpc_type_error);
DEFINE_CONST(XXPC_TYPE_DICTIONARY, _xpc_type_dictionary);
DEFINE_CONST(XXPC_TYPE_ARRAY, _xpc_type_array);
DEFINE_CONST(XXPC_TYPE_STRING, _xpc_type_string);
DEFINE_CONST(XXPC_TYPE_INT64, _xpc_type_int64);
DEFINE_CONST(XXPC_ERROR_CONNECTION_INTERRUPTED,
             _xpc_error_connection_interrupted);
DEFINE_CONST(XXPC_ERROR_CONNECTION_INVALID,
             _xpc_error_connection_invalid);


#define XXPC_ARRAY_APPEND -1
#define XXPC_CONNECTION_MACH_SERVICE_LISTENER 1

#define WRAP(name, args...) \
    x##name args asm("_" #name)

#if !OS_OBJECT_USE_OBJC
void WRAP(xpc_release, (xxpc_object_t));
xxpc_object_t WRAP(xpc_retain, (xxpc_object_t));
#endif
char *WRAP(xpc_copy_description, (xxpc_object_t));

#if OS_OBJECT_USE_OBJC
__attribute__((ns_returns_autoreleased))
#endif
xxpc_type_t WRAP(xpc_get_type, (xxpc_object_t));
xxpc_object_t WRAP(xpc_string_create, (const char *));
const char *WRAP(xpc_string_get_string_ptr, (xxpc_object_t));
int64_t WRAP(xpc_int64_get_value, (xxpc_object_t));
void WRAP(xpc_array_append_value, (xxpc_object_t, xxpc_object_t));
xxpc_object_t WRAP(xpc_array_create, (const xxpc_object_t *, size_t));
size_t WRAP(xpc_array_get_count, (const xxpc_object_t));
xxpc_object_t WRAP(xpc_array_get_value, (xxpc_object_t, size_t));
const char *WRAP(xpc_array_get_string, (xxpc_object_t, size_t));
void WRAP(xpc_array_set_string, (xxpc_object_t, size_t, const char *));
OS_OBJECT_RETURNS_RETAINED
xxpc_object_t WRAP(xpc_dictionary_create, (const char *const *,
                                           const xxpc_object_t *, size_t));
OS_OBJECT_RETURNS_RETAINED
xxpc_object_t WRAP(xpc_dictionary_create_reply, (xxpc_object_t));
bool WRAP(xpc_dictionary_get_bool, (xxpc_object_t, const char *));
const char *WRAP(xpc_dictionary_get_string, (xxpc_object_t, const char *));
uint64_t WRAP(xpc_dictionary_get_uint64, (xxpc_object_t, const char *));
int64_t WRAP(xpc_dictionary_get_int64, (xxpc_object_t, const char *));
xxpc_object_t WRAP(xpc_dictionary_get_value, (xxpc_object_t, const char *));
void WRAP(xpc_dictionary_set_bool, (xxpc_object_t, const char *, bool));
void WRAP(xpc_dictionary_set_string, (xxpc_object_t, const char *, const char *));
void WRAP(xpc_dictionary_set_uint64, (xxpc_object_t, const char *, uint64_t));
void WRAP(xpc_dictionary_set_int64, (xxpc_object_t, const char *, int64_t));
void WRAP(xpc_dictionary_set_value, (xxpc_object_t, const char *, xxpc_object_t));
void WRAP(xpc_dictionary_get_audit_token, (xxpc_object_t, audit_token_t *));

xxpc_connection_t WRAP(xpc_connection_create_mach_service, (const char *,
                                                            dispatch_queue_t,
                                                            uint64_t));
void WRAP(xpc_connection_resume, (xxpc_connection_t));
void WRAP(xpc_connection_set_event_handler, (xxpc_connection_t, xxpc_handler_t));
void WRAP(xpc_connection_send_message_with_reply,
    (xxpc_connection_t, xxpc_object_t, dispatch_queue_t, xxpc_handler_t));
xxpc_object_t WRAP(xpc_connection_send_message_with_reply_sync,
    (xxpc_connection_t, xxpc_object_t));
void WRAP(xpc_connection_send_message, (xxpc_connection_t, xxpc_object_t));
void WRAP(xpc_connection_cancel, (xxpc_connection_t));
int WRAP(xpc_pipe_routine_reply, (xxpc_object_t));


#undef DEFINE_TYPE
#undef DEFINE_CONST
#undef WRAP
#undef DC_CAST
