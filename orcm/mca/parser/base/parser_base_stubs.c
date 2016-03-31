/* Copyright (c) 2016 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/constants.h"
#include "orcm/mca/parser/base/base.h"


int orcm_parser_base_open_file (char const *file)
{
    orcm_parser_active_module_t *active_module = NULL;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return ORCM_ERROR;
        }
        if (NULL != active_module->module->open) {
            return active_module->module->open(file);
        }
    }
    return ORCM_ERROR;
}


int orcm_parser_base_close_file (int file_id)
{
    orcm_parser_active_module_t *active_module = NULL;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return ORCM_ERROR;
        }
        if (NULL != active_module->module->close) {
            return active_module->module->close(file_id);
        }
    }
    return ORCM_ERROR;
}

opal_list_t* orcm_parser_base_retrieve_document (int file_id)
{
    orcm_parser_active_module_t *active_module = NULL;
    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return NULL;
        }
        if (NULL != active_module->module->retrieve_document){
            return active_module->module->retrieve_document(file_id);
        }
    }
    return NULL;
}

opal_list_t* orcm_parser_base_retrieve_section (int file_id, char const* key, char const* name)
{
    orcm_parser_active_module_t *active_module = NULL;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return NULL;
        }
        if (NULL != active_module->module->retrieve_section) {
            return active_module->module->retrieve_section(file_id, key, name);
        }
    }
    return NULL;
}

opal_list_t* orcm_parser_base_retrieve_section_from_list (int file_id, opal_list_item_t *start,
                                                        char const* key, char const* name)
{
    orcm_parser_active_module_t *active_module = NULL;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return NULL;
        }
        if (NULL != active_module->module->retrieve_section_from_list) {
            return active_module->module->retrieve_section_from_list(file_id, start, key, name);
        }
    }
    return NULL;
}

int orcm_parser_base_write_section (int file_id, opal_list_t *input, char const *key,
                                    char const* name, bool overwrite)
{
    orcm_parser_active_module_t *active_module = NULL;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL == active_module) {
            return ORCM_ERROR;
        }
        if (NULL != active_module->module->write_section) {
            return active_module->module->write_section(file_id, input, key, name, overwrite);
        }
    }
    return ORCM_ERROR;
}
