/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CFGI_FILE10_MOCKING_H
#define CFGI_FILE10_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    #include "opal/class/opal_list.h"

    extern char* __real___strdup(const char *s);
    extern int __real_orcm_parser_base_open_file(const char *s);
    extern int __real_orcm_parser_base_close_file(int file_id);
    extern opal_list_t* __real_orcm_parser_base_retrieve_document(int file_id);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef char* (*strdup_fn_t)(const char *s);
typedef int (*orcm_parser_base_open_file_fn_t)(const char *s);
typedef int (*orcm_parser_base_close_file_fn_t)(int file_id);
typedef opal_list_t* (*orcm_parser_base_retrieve_document_fn_t)(int file_id);

class cfgi_file10_mocking
{
    public:
        // Construction
        cfgi_file10_mocking();

        // Public Callbacks
        strdup_fn_t strdup_callback;
        orcm_parser_base_open_file_fn_t orcm_parser_base_open_file_callback;
        orcm_parser_base_close_file_fn_t orcm_parser_base_close_file_callback;
        orcm_parser_base_retrieve_document_fn_t orcm_parser_base_retrieve_document_callback;
};

extern cfgi_file10_mocking cfgi_file10_mocking_object;

#endif // CFGI_FILE10_MOCKING_H
