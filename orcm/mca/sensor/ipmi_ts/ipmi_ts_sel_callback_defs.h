/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SEL_CALLBACK_DEFS_H
#define SEL_CALLBACK_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sel_error_callback_fn_t)(int level, const char* msg);
typedef void (*sel_ras_event_fn_t)(const char* sel_decoded_string, const char* hostname, void* user_object);

#ifdef __cplusplus
}
#endif

#endif
