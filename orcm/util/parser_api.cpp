/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "parser_api.h"

std::map <int,xmlparser*> file_id_table;

BEGIN_C_DECLS

void parser_close_file(int file_id){
   xmlparser *p = file_id_table[file_id];
   file_id_table[file_id] = NULL;
   delete p;
}

int parser_open_file(char const* file){
    xmlparser *p = new xmlparser(file);
    if (p){
        if (ORCM_SUCCESS == p->loadFile()){
            file_id_table[++current_file_id] = p;
            return current_file_id;
        }
    }
    return ORCM_ERR_FILE_OPEN_FAILURE;
}

int parser_retrieve_value(char** ptr_to_value, int file_id, char const* key){
    xmlparser *p = file_id_table[file_id];
    if (p) {
        return p->getValue(ptr_to_value,key);
    }
    return ORCM_ERR_NOT_FOUND;
}

void parser_free_value(char* value){
    if (value){
        free(value);
        value = NULL;
    }
}

int parser_retrieve_array(char*** ptr_to_array, int* size, int file_id,
                          char const* key){
    xmlparser *p = file_id_table[file_id];
    if (p) {
        return p->getArray(ptr_to_array,size,key);
    }
    return ORCM_ERR_NOT_FOUND;
}

void parser_free_array(char** array){
    int size, i;
    if (array) {
        size = sizeof(array)/sizeof(char *);
        for (i=0 ; i<size ; i++)
            parser_free_value(array[i]);
        free(array);
        array = NULL;
    }
}

END_C_DECLS

