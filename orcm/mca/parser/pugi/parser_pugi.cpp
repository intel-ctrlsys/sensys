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


void pugi_write_section(opal_list_t *result,
                        int file_id, char const *key);

    orcm_parser_base_module_t orcm_parser_pugi_module = {
        NULL,
        NULL,
        pugi_open,
        pugi_close,
        pugi_retrieve_section,
        pugi_write_section
    };

static map<int,parser_t*> file_id_table;
static int current_file_id=0;

bool is_file_id_valid(int file_id){
    if (0 >= file_id_table.count(file_id )){
        return false;
    }
    return true;
}

int pugi_open(char const *file)
{
    parser_t *p= new parser_t(file);
    if (NULL != p){
        if (ORCM_SUCCESS == p->loadFile()){
            file_id_table[++current_file_id] = p;
            return current_file_id;
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

opal_list_t* pugi_retrieve_section(int file_id, opal_list_item_t *start,
                                   char const* key, char const* name)
{
    if (!is_file_id_valid(file_id)){
        return NULL;
    }
    parser_t *p = file_id_table[file_id];
    return p->retrieveSection(start,key,name);
}

void pugi_write_section(opal_list_t *result,
                        int file_id, char const *key)
{

}
END_C_DECLS
