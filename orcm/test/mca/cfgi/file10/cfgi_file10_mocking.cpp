/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "cfgi_file10_mocking.h"

#include <iostream>
using namespace std;

cfgi_file10_mocking cfgi_file10_mocking_object;


extern "C" { // Mocking must use correct "C" linkages.

    char* __wrap___strdup(const char *s)
    {
        if(NULL == cfgi_file10_mocking_object.strdup_callback) {
            return __real___strdup(s);
        } else {
            return cfgi_file10_mocking_object.strdup_callback(s);
        }
    }

    int __wrap_orcm_parser_base_open_file (char const *s)
    {
        if(NULL == cfgi_file10_mocking_object.orcm_parser_base_open_file_callback) {
            return __real_orcm_parser_base_open_file(s);
        } else {
            return cfgi_file10_mocking_object.orcm_parser_base_open_file_callback(s);
        }
    }

    int __wrap_orcm_parser_base_close_file (int file_id)
    {
        if(NULL == cfgi_file10_mocking_object.orcm_parser_base_close_file_callback) {
            return __real_orcm_parser_base_close_file(file_id);
        } else {
            return cfgi_file10_mocking_object.orcm_parser_base_close_file_callback(file_id);
        }
    }

    opal_list_t* __wrap_orcm_parser_base_retrieve_document (int file_id)
    {
        if(NULL == cfgi_file10_mocking_object.orcm_parser_base_retrieve_document_callback) {
            return __real_orcm_parser_base_retrieve_document(file_id);
        } else {
            return cfgi_file10_mocking_object.orcm_parser_base_retrieve_document_callback(file_id);
        }
    }

}; // extern "C"

cfgi_file10_mocking::cfgi_file10_mocking() :
    strdup_callback(NULL),
    orcm_parser_base_open_file_callback(NULL),
    orcm_parser_base_close_file_callback(0),
    orcm_parser_base_retrieve_document_callback(0)
{
}
