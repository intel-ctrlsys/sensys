/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <ctype.h>
#include <map>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "parser_pugi.h"
#include "pugi_impl.h"

using namespace std;

typedef pugi_impl parser_t;

BEGIN_C_DECLS

orcm_parser_base_module_t orcm_parser_pugi_module = {
    NULL,
    pugi_finalize,
    pugi_open,
    pugi_close,
    pugi_retrieve_document,
    pugi_retrieve_section,
    pugi_retrieve_section_from_list,
    pugi_write_section
};

static map<int,parser_t*> file_id_table;
static int current_file_id=0;

inline bool is_file_id_valid(int file_id){
    return !(0 >= file_id_table.count(file_id));
}

void print_error_msg(const char *msg)
{
    fprintf(stderr, "ERROR: %s : pugi_parser: %s \n", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), msg);
}


void pugi_finalize()
{
    for(map<int,parser_t*>::iterator it=file_id_table.begin();
        it != file_id_table.end() ; it++){
        delete it->second;
    }
    file_id_table.clear();
    current_file_id = 0;
}

int pugi_open(char const *file)
{
    parser_t *p= new parser_t(file);
    if (NULL != p){
        try {
            if (ORCM_SUCCESS == p->loadFile()){
                file_id_table[++current_file_id] = p;
                return current_file_id;
            }
        } catch (exception &e){
            print_error_msg(e.what());
        }
        delete p;
    }
    return ORCM_ERR_FILE_OPEN_FAILURE;
}

int pugi_close(int file_id)
{
    if (!is_file_id_valid(file_id)){
        return ORCM_ERROR;
    }
    parser_t *p = file_id_table[file_id];
    file_id_table.erase(file_id);
    try{
        delete p;
    } catch (exception e) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

parser_t* get_parser_object(int file_id)
{
    if (is_file_id_valid(file_id)){
        return file_id_table[file_id];
    }
    return NULL;
}

opal_list_t* pugi_retrieve_document(int file_id)
{
    parser_t *p = get_parser_object(file_id);
    if (NULL == p){
        return NULL;
    }
    return p->retrieveDocument();
}

opal_list_t* pugi_retrieve_section(int file_id, char const* key, char const* name)
{
    parser_t *p = get_parser_object(file_id);
    if (NULL == p){
        return NULL;
    }
    return p->retrieveSection(key,name);
}

opal_list_t* pugi_retrieve_section_from_list(int file_id, opal_list_item_t *start,
                                             char const* key, char const* name)
{
    parser_t *p = get_parser_object(file_id);
    if (NULL == p){
        return NULL;
    }
    return p->retrieveSectionFromList(start,key,name);
}

int pugi_write_section(int file_id, opal_list_t *input,
                       char const *key, char const* name, bool overwrite)
{
    parser_t *p = get_parser_object(file_id);
    if (NULL == p){
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }
    return p->writeSection(input, key, name, overwrite);
}
END_C_DECLS
