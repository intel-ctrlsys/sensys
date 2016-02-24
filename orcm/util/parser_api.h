/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_API_H
#define PARSER_API_H

#ifndef BEGIN_C_DECLS
#define BEGIN_C_DECLS extern "C" {
#define END_C_DECLS }
#endif

#include "xmlparser.h"
#include <stdlib.h>
#include <map>

BEGIN_C_DECLS

extern void parser_close_file(int file_id);
extern int  parser_open_file(char const* file);
extern int  parser_retrieve_value(char** ptr_to_value, int file_id,
                                  char const* key);
extern void parser_free_value(char* value);
extern int  parser_retrieve_array(char*** ptr_to_array, int* size, int file_id,
                                  char const* key);
extern void parser_free_array(char** array);

static int current_file_id = 0;

END_C_DECLS

#endif //PARSER_API_H

