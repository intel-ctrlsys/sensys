/*
 * Copyright (c) 2016      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdlib.h>
#include <regex.h>

#include "orte/util/regex.h"
#include "orte/util/show_help.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/util/utils.h"

#include "orcm/mca/parser/parser.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/file30/cfgi_file30_helpers.h"

#define TAG value.key
#define VALUE value.data.string
#define SUBLIST value.data.ptr

int number_of_clusters;
int schedulers_count;

int stringMatchRegex(char* str, char* pattern) {
  regex_t regex_comp;
  regcomp(&regex_comp, pattern, REG_ICASE | REG_EXTENDED);
  int rv = !regexec(&regex_comp, str, 0, NULL, 0);
  regfree(&regex_comp);
  return rv;
}

int print_pugi_opal_list (opal_list_t *root) {
    if (NULL == root){
        return ORCM_ERR_SILENT;
    }

    orcm_value_t *ptr;
    OPAL_LIST_FOREACH(ptr,root,orcm_value_t){
        opal_output(0,"\titem: %s",ptr->value.key);
        if (ptr->value.type == OPAL_STRING){
            opal_output(0,"\tvalue: %s",ptr->value.data.string);
        } else {
            opal_output(0,"\tchildren: ");
            print_pugi_opal_list((opal_list_t*)ptr->value.data.ptr);
            opal_output(0,"\tend-list\n");
        }
    }

    return ORCM_SUCCESS;
}

int check_file_exist(void) {
    FILE *fp = fopen(orcm_cfgi_base.config_file, "r");
    if (NULL == fp) {
        orte_show_help("help-orcm-cfgi.txt", "site-file-not-found",
                       true, orcm_cfgi_base.config_file);
        return ORCM_ERR_SILENT;
    }

    if (0 != fclose(fp)) {
        opal_output(0, "FAIL to PROPERLY CLOSE THE CFGI XML FILE");
        return ORCM_ERR_SILENT;
    }

    fp = NULL;
    return ORCM_SUCCESS;
}

orcm_value_t * get_child(opal_list_t* root, char* child_name) {
    orcm_value_t *parent = GET_ITEM_FROM_OPAL_LIST(root, orcm_value_t);
    orcm_value_t *child = NULL;
    opal_list_t* children = (opal_list_t*) parent->SUBLIST;

    if (NULL != children) {
        OPAL_LIST_FOREACH(child, children, orcm_value_t) {
            if (0 == strcasecmp(child->TAG, child_name)) {
                return child;
            }
        }
    }
    return NULL;
}

void get_type_and_name(orcm_value_t * parent, char** type, char** name){
    orcm_value_t *child = NULL;
    opal_list_t* children = (opal_list_t*) parent->SUBLIST;
    *type = NULL;
    *name = NULL;

    if (NULL != children) {
        OPAL_LIST_FOREACH(child, children, orcm_value_t) {
            if (0 == strcasecmp(child->TAG, TXname)) {

                *name = child->VALUE;

            } else if (0 == strcasecmp(child->TAG, TXtype)) {

                *type = child->VALUE;

            }
        }
    }
}

char * tolower_cstr(char * in) {
    char * t = in;
    while (NULL != t && '\0' != *t) {
        if ('A' >= *t && 'Z' <= *t) {
            *t -= 'A';
            *t += 'a';
        }
        ++t;
    }
    return in;
}

orcm_cfgi_xml_parser_t * build_cfgi_xml_parser_object(char* name) {
    orcm_cfgi_xml_parser_t * return_object = OBJ_NEW(orcm_cfgi_xml_parser_t);

    if (NULL != name) {
        return_object->name = strdup(name);
        tolower_cstr(return_object->name);
    }
    return return_object;
}

int merge_controller_with_node(opal_list_t* source, opal_list_t *target) {

    orcm_value_t *source_item;
    orcm_cfgi_xml_parser_t *attribute;

    OPAL_LIST_FOREACH(source_item, source, orcm_value_t) {
        if(OPAL_PTR == source_item->value.type){
            opal_output(0, "ERROR: Controller cannot have a child item");
            return ORCM_ERR_BAD_PARAM;
        }

        if (0 != strcasecmp(source_item->TAG, TXhost)){
            attribute = build_cfgi_xml_parser_object(source_item->TAG);
            opal_argv_append_nosize(&attribute->value, source_item->VALUE);
            opal_list_append(target, &attribute->super);
        }
    }
    return ORCM_SUCCESS;
}

int parse_controller_tag(opal_list_t* source, opal_list_t *target) {
    orcm_value_t *source_item;
    orcm_cfgi_xml_parser_t *target_item, *attribute;

    target_item = build_cfgi_xml_parser_object(TXcontrol);

    OPAL_LIST_FOREACH(source_item, source, orcm_value_t) {
        if(OPAL_PTR == source_item->value.type){
            opal_output(0, "ERROR: Controller cannot have a child item");
            return ORCM_ERR_BAD_PARAM;
        }

        if (0 == strcasecmp(source_item->TAG, TXhost))
            opal_argv_append_nosize(&target_item->value, source_item->VALUE);
        else {
            attribute = build_cfgi_xml_parser_object(source_item->TAG);
            opal_argv_append_nosize(&attribute->value, source_item->VALUE);
            opal_list_append(&target_item->subvals, &attribute->super);
        }
    }

    opal_list_append(target, &target_item->super);

    return ORCM_SUCCESS;
}

int parse_cluster_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type){

    number_of_clusters++;

    if(1 < number_of_clusters){
        opal_output(0,"ERROR: %s tag must have a single cluster junction", parent_type);
        return ORCM_ERR_BAD_PARAM;
    }

    return parse_junction_tag(source, target, name, FDcluster);
}

int parse_row_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type){

    if(0 != strcasecmp(FDcluster,parent_type)){
        opal_output(0,
            "ERROR: Only a %s junction can contain a %s junction: %s found.",
            FDcluster, FDrow, parent_type);
        return ORCM_ERR_BAD_PARAM;
    }

    return parse_junction_tag(source, target, name, FDrow);
}

int parse_rack_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type){

    if(0 != strcasecmp(FDrow,parent_type)){
        opal_output(0,
            "ERROR: Only a %s junction can contain a %s junction: %s found.",
            FDrow, FDrack, parent_type);
        return ORCM_ERR_BAD_PARAM;
    }

    return parse_junction_tag(source, target, name, FDrack);
}

int parse_node_junction(opal_list_t* source, opal_list_t *target, char *name, char *parent_type){

    if(0 != strcasecmp(FDrack,parent_type)){
        opal_output(0,
            "ERROR: Only a %s junction can contain a %s junction: %s found.",
            FDrack, FDnode, parent_type);
        return ORCM_ERR_BAD_PARAM;
    }

    return parse_junction_tag(source, target, name, FDnode);
}

int select_parse_junction_tag(orcm_value_t* source_item, opal_list_t* target, char* parent_type){
    char *item_type, *item_name;
    int return_value = ORCM_SUCCESS;

    get_type_and_name(source_item, &item_type, &item_name);

    if(NULL == item_name){
        opal_output(0,"ERROR: A junction is missing its name");
        return_value = ORCM_ERR_BAD_PARAM;
    }

    if(NULL == item_type){
        opal_output(0,"ERROR: A junction is missing its type");
        return_value = ORCM_ERR_BAD_PARAM;

    }

    if(0 == strcasecmp(FDnode, parent_type)){
        opal_output(0,
                "ERROR: A node junction must contain no other junction: %s found.", item_type);
        return_value = ORCM_ERR_BAD_PARAM;

    }

    if(return_value != ORCM_SUCCESS){
        return return_value;
    }

    if(0 == strcasecmp(FDcluster,item_type)){

        return_value = parse_cluster_junction(source_item->SUBLIST,target, item_name, parent_type);

    } else if(0 == strcasecmp(FDrow,item_type)){

        return_value = parse_row_junction(source_item->SUBLIST,target, item_name, parent_type);

    } else if(0 == strcasecmp(FDrack,item_type)){

        return_value = parse_rack_junction(source_item->SUBLIST,target, item_name, parent_type);

    } else if(0 == strcasecmp(FDnode,item_type)){

        return_value = parse_node_junction(source_item->SUBLIST,target, item_name, parent_type);
    }

    return return_value;
}

int parse_junction_tag(opal_list_t* source, opal_list_t *target, char *name, char *type) {

    orcm_value_t *source_item;
    orcm_cfgi_xml_parser_t *target_item;
    int return_value = ORCM_SUCCESS;
    int junction_counter = 0;

    target_item = build_cfgi_xml_parser_object(NULL);
    target_item->name = strdup(type);
    opal_argv_append_nosize(&target_item->value, name);

    OPAL_LIST_FOREACH(source_item, source, orcm_value_t) {
        if (0 == strcasecmp(source_item->TAG, TXjunction)){

            return_value = select_parse_junction_tag(source_item, &target_item->subvals, type);

            if(ORCM_SUCCESS == return_value)
                junction_counter++;

        } else if (0 == strcasecmp(source_item->TAG, TXcontrol)){
            if(0 != strcasecmp(FDnode,type)){
                return_value = parse_controller_tag(source_item->SUBLIST, &target_item->subvals);
            } else{
                return_value = merge_controller_with_node(source_item->SUBLIST,&target_item->subvals);
            }
        }

        if(ORCM_SUCCESS != return_value){
            return return_value;
        }
    }

    if(0 == junction_counter && 0 != strcasecmp(FDnode,type)){
        opal_output(0,
            "ERROR: A %s junction must contain at least one child junction: 0 found.", type);
        return_value = ORCM_ERR_BAD_PARAM;
    }

    opal_list_append(target, &target_item->super);

    return return_value;
}

int parse_scheduler_tag(opal_list_t* source, opal_list_t *target) {

    orcm_value_t *source_item;
    orcm_cfgi_xml_parser_t *target_item, *attribute;
    int return_code = ORCM_SUCCESS;

    target_item = build_cfgi_xml_parser_object(TXscheduler);

    OPAL_LIST_FOREACH(source_item, source, orcm_value_t) {

        if(OPAL_PTR == source_item->value.type){
            opal_output(0,
                "ERROR: Scheduler cannot have a child item");
            return ORCM_ERR_BAD_PARAM;
        }

        if (0 == strcasecmp(source_item->TAG, TXshost))
            attribute = build_cfgi_xml_parser_object(FDnode);
        else
            attribute = build_cfgi_xml_parser_object(source_item->TAG);

        opal_argv_append_nosize(&attribute->value, source_item->VALUE);
        opal_list_append(&target_item->subvals, &attribute->super);
    }

    schedulers_count++;

    if(1 != schedulers_count){
        opal_output(0,
                        "ERROR: Only a single scheduler is allowed at a time");
        return_code = ORCM_ERR_BAD_PARAM;
    }

    opal_list_append(target, &target_item->super);
    return return_code;
}

int parse_configuration_tag(opal_list_t* source, opal_list_t *target) {
    orcm_value_t *source_item;
    int return_code = ORCM_SUCCESS;
    char *type;
    char *name;
    int clusters_found = 0;

    OPAL_LIST_FOREACH(source_item, source, orcm_value_t) {
        return_code = ORCM_SUCCESS;
        if (0 == strcasecmp(source_item->TAG, TXjunction)) {

            get_type_and_name(source_item, &type, &name);
            if(NULL == name){
                opal_output(0,"ERROR: A junction is missing its name");
                return_code = ORCM_ERR_BAD_PARAM;
            }

            if(NULL == type){
                opal_output(0,"ERROR: A junction is missing its type");
                return_code = ORCM_ERR_BAD_PARAM;

            } else if(0 == strcasecmp(type,FDcluster)){
                clusters_found++;
            }

            if(ORCM_SUCCESS == return_code){
                return_code = select_parse_junction_tag(source_item, target, TXconfig);
            }

        } else if (0 == strcasecmp(source_item->TAG, TXscheduler)) {

            return_code = parse_scheduler_tag(source_item->SUBLIST, target);
        }

        if(ORCM_SUCCESS != return_code){
            break;
        }
    }

    if(1 != clusters_found){
        opal_output(0,"ERROR: Configuration tag must have a single cluster junction at the top");
        return_code = ORCM_ERR_BAD_PARAM;
    }

    return return_code;
}

int convert_to_orcm_cfgi_xml_parser_t_list(opal_list_t* source, opal_list_t *target) {
    orcm_value_t *source_item = GET_ITEM_FROM_OPAL_LIST(source, orcm_value_t);
    orcm_cfgi_xml_parser_t *target_item;
    number_of_clusters = 0;
    schedulers_count = 0;

    if (0 != strcasecmp(source_item->TAG, TXconfig)) {
        opal_output(0,"ERROR: Configuration tag must be at the top of the file.");
        return ORCM_ERR_BAD_PARAM;
    }

    target_item = build_cfgi_xml_parser_object(source_item->TAG);
    opal_list_append(target, &target_item->super);

    return parse_configuration_tag(source_item->SUBLIST, &target_item->subvals);
}

int check_lex_port_field(char *field_value) {
    long number = 0;
    char *endptr = NULL;
    const int base = 10;

    number = strtol(field_value, &endptr, base);
    if ('\0' != *endptr) {
        opal_output(0,"ERROR: The value of a port is not a valid integer");
        return ORCM_ERR_BAD_PARAM;
    } else if (0 > number || USHRT_MAX < number) {
        opal_output(0, "ERROR: The value of a port is not in an acceptable range "
                       "(0<=n<=SHRT_MAX): %ld", number);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

int check_record_field(char *field_value, int *record) {
    if ( 0 == strcasecmp(FDrecord, field_value) ) {
        (*record)++;
        if (*record > 1) {
            opal_output(0,"ERROR: Can't set up more than 1 RECORD string \n");
            return ORCM_ERR_BAD_PARAM;
        }
    }else{
        opal_output(0, "ERROR: Unknown data content for the XML tag ->role");
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

int check_aggregator_yes_no_field(char *field_value) {
    int y = 0;
    int n = 0;
    y = strcasecmp("yes", field_value);
    n = strcasecmp("no" , field_value);
    if ( !( 0 == y || 0 == n) ) {
        opal_output(0, "ERROR: Aggregator values must be yes or no only\n");
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

int check_lex_tags_and_field(opal_list_t *root) {
    char *key = NULL;
    char *data = NULL;
    char *next = NULL;
    orcm_value_t *ptr = NULL;
    int record_count = 0;
    static int aggs_count = 0;

    OPAL_LIST_FOREACH(ptr, root, orcm_value_t) {
        key = ptr->value.key;
        data = ptr->value.data.string;
        next = ptr->value.data.ptr;

        if (0 == strcasecmp(TXrole, key)) {
            if (ORCM_SUCCESS != check_record_field(data, &record_count)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (0 == strcasecmp(TXaggregat, key)) {
            if (0 == strcasecmp("yes", data)) {
                ++aggs_count;
            }
            if (ORCM_SUCCESS != check_aggregator_yes_no_field(data)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (0 == strcasecmp(TXport, key)) {
            if (ORCM_SUCCESS != check_lex_port_field(data)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (OPAL_STRING != ptr->value.type) {
            /* Iterating over the childrens */
            if (ORCM_SUCCESS != check_lex_tags_and_field((opal_list_t*)next)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }
    }

    if (0 >= aggs_count) {
       opal_output(0,"ERROR: Need at least one aggregator configuration\n");
       return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

/* A singleton item is one where only one instance
   of that item can be present in the current context.
   Returns 1 for singleton item
   Returns 0 otherwise.
*/
int is_singleton(const char * in_tagtext)
{
    const char *t = in_tagtext;

    /*First a quick check than a fuller one*/
    if (NULL == t) {
        return 0;
    }

    switch (t[0]) {
        case 'a':
        case 'A':
            if (0 == strcasecmp(t,TXaggregat)) {
                return 1;
            }
            break;
        case 'c':
        case 'C':
            if (0 == strcasecmp(t,TXcontrol) ||
                0 == strcasecmp(t,TXcount) ||
                0 == strcasecmp(t,TXcores) ||
                0 == strcasecmp(t,TXcreation)) {
                return 1;
            }
            break;
        case 'e':
        case 'E':
            if (0 == strcasecmp(t,TXenvar)) {
                return 1;
            }
            break;
        case 'h':
        case 'H':
            if (0 == strcasecmp(t,TXhost)) {
                return 1;
            }
            break;
        case 'l':
        case 'L':
            if (0 == strcasecmp(t,TXlog)) {
                return 1;
            }
            break;
        case 'n':
        case 'N':
            if (0 == strcasecmp(t,TXname)) {
                return 1;
            }
            break;
         case 'p':
         case 'P':
             if (0 == strcasecmp(t,TXport)) {
                 return 1;
             }
             break;
         case 'r':
         case 'R':
             if (0 == strcasecmp(t,TXrole)) {
                 return 1;
             }
             break;
         case 's':
         case 'S':
             if (0 == strcasecmp(t,TXscheduler) ||
                 0 == strcasecmp(t,TXshost)) {
                 return 1;
             }
             break;
         case 't':
         case 'T':
             if ( ! strcasecmp(t,TXtype)) {
                 return 1;
             }
             break;
         case 'v':
         case 'V':
             if (0 == strcasecmp(t,TXversion)) {
                 return 1;
             }
             break;
        default:
            break;
    }
    return 0;
}

int check_duplicate_singleton(opal_list_t *root) {
    orcm_value_t *ptr = NULL;
    char **singleton_list = {NULL};
    static int error = ORCM_SUCCESS;
    static int mem_counter = 1;
    static int singleton_counter = 0;

    singleton_list = (char **)malloc(sizeof(char*));
    OPAL_LIST_FOREACH(ptr, root, orcm_value_t) {
        if (is_singleton(ptr->value.key) ) {
            singleton_list = (char **)realloc(singleton_list, sizeof(char*) * mem_counter);
            if (NULL != singleton_list) {
                singleton_list[singleton_counter] = ptr->value.key;
                for (int i = 0; i < singleton_counter; i++) {
                    if (0 == strcasecmp(singleton_list[i], ptr->value.key)) {
                        opal_output(0, "ERROR: More than one instance of \"%s\" was found",
                                       ptr->value.key);
                        error = ORCM_ERR_BAD_PARAM;
                    }
                }
                ++mem_counter;
                ++singleton_counter;
            } else {
                opal_output(0, "ERROR: couldn't allocate more memory");
                SAFEFREE(singleton_list);
                error = ORCM_ERROR;
            }
        }
        if (OPAL_STRING != ptr->value.type) {
            singleton_counter = 0;
            check_duplicate_singleton((opal_list_t*)ptr->value.data.ptr);
        }
    }
    SAFEFREE(singleton_list);
    return error;
}

