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

#include <ctype.h>
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
int singleton_error;

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
            opal_output(0,"ERROR: Can't set up more than 1 RECORD role \n");
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

int is_not_ignored(char* tag){

    if(0 == strcasecmp(TXjunction,tag)||
       0 == strcasecmp(TXcontrol,tag) ||
       0 == strcasecmp(TXconfig,tag)  ||
       0 == strcasecmp(TXscheduler,tag)){

        return 1;
    }
    return 0;
}

int check_lex_tags_and_field(opal_list_t *root) {
    int role_count = 0;
    int aggs_count = 0;
    int ret = ORCM_SUCCESS;

    if (NULL == root){
        opal_output(0,"ERROR: empty configuration tree\n");
        return ORCM_ERR_SILENT;
    }

    ret = search_lex_tags_and_field(root, &role_count, &aggs_count);

    if (0 >= aggs_count) {
        opal_output(0,"ERROR: Need at least one aggregator configuration\n");
        return ORCM_ERR_BAD_PARAM;
    }

    if (0 >= role_count) {
        opal_output(0,"ERROR: Need at least one role configuration\n");
        return ORCM_ERR_BAD_PARAM;
    }

    return ret;

}

int search_lex_tags_and_field(opal_list_t *root, int *role, int *aggs) {
    orcm_value_t *ptr = NULL;
    int record_count = 0;

    OPAL_LIST_FOREACH(ptr, root, orcm_value_t) {

        if (0 == strcasecmp(TXrole, ptr->TAG)) {
            (*role)++;
            if (ORCM_SUCCESS != check_record_field(ptr->VALUE, &record_count)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (0 == strcasecmp(TXaggregat, ptr->TAG)) {
            if (0 == strcasecmp("yes", ptr->VALUE)) {
                (*aggs)++;
            }
            if (ORCM_SUCCESS != check_aggregator_yes_no_field(ptr->VALUE)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (0 == strcasecmp(TXport, ptr->TAG)) {
            if (ORCM_SUCCESS != check_lex_port_field(ptr->VALUE)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }

        if (OPAL_STRING != ptr->value.type && is_not_ignored(ptr->TAG)) {
            if (ORCM_SUCCESS != search_lex_tags_and_field((opal_list_t*)ptr->SUBLIST, role, aggs)) {
                return ORCM_ERR_BAD_PARAM;
            }
        }
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
    if (NULL == t) {
        return 0;
    }

    int first_letter = t[0];
    first_letter = tolower(first_letter);

    switch (first_letter) {
        case 'a':
            if (0 == strcasecmp(t,TXaggregat)) {
                return 1;
            }
            break;
        case 'c':
            if (0 == strcasecmp(t,TXcontrol) ||
                0 == strcasecmp(t,TXcount) ||
                0 == strcasecmp(t,TXcores) ||
                0 == strcasecmp(t,TXcreation)) {
                return 1;
            }
            break;
        case 'e':
            if (0 == strcasecmp(t,TXenvar)) {
                return 1;
            }
            break;
        case 'h':
            if (0 == strcasecmp(t,TXhost)) {
                return 1;
            }
            break;
        case 'l':
            if (0 == strcasecmp(t,TXlog)) {
                return 1;
            }
            break;
        case 'n':
            if (0 == strcasecmp(t,TXname)) {
                return 1;
            }
            break;
         case 'p':
             if (0 == strcasecmp(t,TXport)) {
                 return 1;
             }
             break;
         case 'r':
             if (0 == strcasecmp(t,TXrole)) {
                 return 1;
             }
             break;
         case 's':
             if (0 == strcasecmp(t,TXscheduler) ||
                 0 == strcasecmp(t,TXshost)) {
                 return 1;
             }
             break;
         case 't':
             if ( ! strcasecmp(t,TXtype)) {
                 return 1;
             }
             break;
         case 'v':
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
    int mem_counter = 1;

    singleton_error = ORCM_SUCCESS;
    return search_singletons(root, &mem_counter);
}

int search_singletons(opal_list_t *root, int *mem_counter) {
    orcm_value_t *ptr = NULL;
    char **singleton_list = {NULL};
    int singleton_counter = 0;

    singleton_list = (char **)malloc(sizeof(char*));
    OPAL_LIST_FOREACH(ptr, root, orcm_value_t) {
        if (is_singleton(ptr->TAG)) {
            singleton_list = (char **)realloc(singleton_list, sizeof(char*)*(*mem_counter));
            if (NULL != singleton_list) {
                singleton_list[singleton_counter] = ptr->TAG;
                for (int i = 0; i < singleton_counter; i++) {
                    if (0 == strcasecmp(singleton_list[i], ptr->TAG)) {
                        opal_output(0, "ERROR: More than one instance of \"%s\" was found", ptr->value.key);
                        singleton_error = ORCM_ERR_BAD_PARAM;
                    }
                }
                (*mem_counter)++;
                ++singleton_counter;
            } else {
                opal_output(0, "ERROR: couldn't allocate more memory");
                SAFEFREE(singleton_list);
                return ORCM_ERROR;
            }
        }

        if (OPAL_STRING != ptr->value.type && is_not_ignored(ptr->TAG)) {
            if (ORCM_SUCCESS != (singleton_error = search_singletons((opal_list_t*)ptr->SUBLIST, mem_counter))){
                SAFEFREE(singleton_list);
                return singleton_error;
            }
        }
    }
    SAFEFREE(singleton_list);
    return singleton_error;
}

int cfgi30_check_configure_hosts_ports(opal_list_t *elements_list)
{
    orcm_value_t *element = NULL;
    char **hosts = NULL;
    int *ports = NULL;
    int res = ORCM_SUCCESS;

    hosts = (char**)malloc(sizeof(char*));
    if( NULL != hosts ){
        hosts[0] = NULL;
        element = (orcm_value_t *)elements_list->opal_list_sentinel.opal_list_next;
        elements_list = (opal_list_t*)element->value.data.ptr;

        OPAL_LIST_FOREACH(element, elements_list, orcm_value_t){
            if( 0 == strcasecmp(TXjunction, element->value.key) ){
                res = cfgi30_check_junction_hosts_ports(
                            (opal_list_t*)element->value.data.ptr, &hosts,
                            &ports, NULL);
            } else if( 0 == strcasecmp(TXscheduler, element->value.key) ){
                res = cfgi30_check_scheduler_hosts_ports(
                            (opal_list_t*)element->value.data.ptr, &hosts,
                            &ports);
            }

            if( ORCM_SUCCESS != res ) break;
        }
    } else {
        res = ORCM_ERR_OUT_OF_RESOURCE;
    }

    cfgi30_free_string_array(&hosts);
    if( NULL != ports ) SAFEFREE(ports);

    return res;
}

int cfgi30_check_scheduler_hosts_ports(opal_list_t *scheduler, char ***hosts,
                                       int **ports)
{
    char *host = NULL;
    int port = -1;
    int res = ORCM_SUCCESS;

    res = cfgi30_check_controller_hosts_ports(scheduler, &host, &port);
    if( ORCM_SUCCESS == res ){
        res = cfgi30_add_uniq_host_port(hosts, ports, &host, &port);
    }

    return res;
}

int cfgi30_check_junction_hosts_ports(opal_list_t *junction, char ***hosts,
                                      int **ports, char **lastName)
{
    orcm_value_t *element = NULL;
    opal_list_t **next_junctions = (opal_list_t**)malloc( sizeof(opal_list_t*) );
    int junctions = 0;
    char *name = NULL;
    char *host = NULL;
    int port = -1;
    char is_node = 2;
    int res = ORCM_SUCCESS;

    if( NULL == next_junctions ) res = ORCM_ERR_OUT_OF_RESOURCE;
    else next_junctions[0] = NULL;

    OPAL_LIST_FOREACH(element, junction, orcm_value_t){
        if( ORCM_SUCCESS != res ) break;

        if( 0 == strcasecmp(TXjunction, element->value.key) ){
            next_junctions = (opal_list_t**)realloc( next_junctions,
                                       sizeof(opal_list_t*) * (junctions + 2));
            if( NULL != next_junctions ){
                next_junctions[junctions] = (opal_list_t*)element->value.data.ptr;
                junctions++;
                next_junctions[junctions] = NULL;
            } else {
                res = ORCM_ERR_OUT_OF_RESOURCE;
            }
        } else if( 0 == strcasecmp(TXcontrol, element->value.key) ){
            res = cfgi30_check_controller_hosts_ports(
                        (opal_list_t*)element->value.data.ptr, &host, &port);
        } else if( 0 == strcasecmp(TXname, element->value.key) ){
            if( name != NULL ) SAFEFREE(name);
            name = strdup(element->value.data.string);
            if( NULL == name ) res = ORCM_ERR_OUT_OF_RESOURCE;
        } else if( 0 == strcasecmp(TXtype, element->value.key) ){
            if( 0 == strcasecmp(FDnode, element->value.data.string) ) is_node = 1;
            else is_node = 0;
        }
    }

    if( ORCM_SUCCESS == res ){
        if( NULL != name ){
            if( 2 != is_node ){
                res = cfgi30_check_unique_hosts_ports(hosts, ports, &name,
                            &host, &port, lastName, next_junctions, is_node);
            } else {
                opal_output(0,"ERROR: A junction is missing it's type");
                res = ORCM_ERR_BAD_PARAM;
            }
        } else {
            opal_output(0,"ERROR: A junction is missing it's name");
            res = ORCM_ERR_BAD_PARAM;
        }
    }

    SAFEFREE(name);
    SAFEFREE(next_junctions);
    return res;
}

int cfgi30_check_controller_hosts_ports(opal_list_t *controller, char **host,
                                        int *port){
    orcm_value_t *element = NULL;
    int res = ORCM_SUCCESS;

    OPAL_LIST_FOREACH(element, controller, orcm_value_t){
        if( 0 == strcasecmp(TXhost, element->value.key)
            || 0 == strcasecmp(TXshost, element->value.key)){
            (*host) = element->value.data.string;
        } else if( 0 == strcasecmp(TXport, element->value.key) ){
            (*port) = atoi(element->value.data.string);
        }
    }

    if( NULL == (*host) ){
        opal_output(0,"ERROR: Controller/Scheduler with missing (s)host field");
        res = ORCM_ERR_BAD_PARAM;
    } else if( -1 == (*port) ){
        opal_output(0,"ERROR: Controller/Scheduler with missing port field");
        res = ORCM_ERR_BAD_PARAM;
    }

    return res;
}

int cfgi30_check_unique_hosts_ports(char ***hosts, int **ports, char **name,
                                    char **host, int *port, char **lastName,
                                    opal_list_t **next_junctions, char is_node)
{
    int res = ORCM_SUCCESS;
    int regex_res;
    int str_pos = 0;
    char *expandable = NULL;
    char **expanded = NULL;
    char is_regex = 0;
    regmatch_t list_matches[4];
    regex_t regex_comp_oregex;


    res = cfgi30_expand_name_at(name, lastName);
    if( ORCM_SUCCESS == res ){
        regcomp(&regex_comp_oregex,
                "(([^[,[:space:]]*\\[[[:digit:]]+:"
                        "[[:digit:]]+-[[:digit:]]+\\][^,[:space:]]*)"
                    "|([^,[:space:]]+)),?",
                REG_EXTENDED);

        while( ORCM_SUCCESS == res
               && !(regex_res = regexec(&regex_comp_oregex, (*name) + str_pos,
                                        4, list_matches, 0)) ) {
            if( 0 < list_matches[2].rm_eo ){
                is_regex = 1;
                expandable = strndup( (*name) + str_pos + list_matches[2].rm_so,
                                      (int)(list_matches[2].rm_eo
                                          - list_matches[2].rm_so));

                if( NULL != expandable){
                    res = orte_regex_extract_node_names(expandable, &expanded);
                    free(expandable);
                } else {
                    res = ORCM_ERR_OUT_OF_RESOURCE;
                }
            } else if( 0 < list_matches[3].rm_eo ){
                if( (*name)[list_matches[0].rm_eo] == ',' ){
                    is_regex = 1;
                }

                expandable = strndup( (*name) + str_pos + list_matches[3].rm_so,
                                      (int)(list_matches[3].rm_eo
                                          - list_matches[3].rm_so));
                expanded = (char**)malloc( sizeof(char*) * 2 );
                if( NULL != expanded && NULL != expandable ){
                    expanded[0] = expandable;
                    expanded[1] = NULL;
                } else {
                    res = ORCM_ERR_OUT_OF_RESOURCE;
                }
            }

            if( ORCM_SUCCESS == res ){
                if( !is_regex
                    || (NULL != (*host) && 0 != strcasecmp(*host, "@")) ){
                    res = cfgi30_add_valid_noregex_host_port(hosts, ports, host,
                                port, &expanded, is_node);
                }

                if( ORCM_SUCCESS == res ){
                    str_pos += list_matches[0].rm_eo;
                    res = cfgi30_add_valid_regex_hosts_ports(hosts, ports, host,
                                port, &expanded, is_node, is_regex,
                                next_junctions);
                }
            }

            cfgi30_free_string_array(&expanded);
        }
        regfree(&regex_comp_oregex);
    }

    return res;
}

int cfgi30_expand_name_at(char **name, char **lastName)
{
    char *at_pos = strchr(*name, '@');
    char *tmp = NULL;
    int ln_size, n_size;
    int res = ORCM_SUCCESS;

    if( NULL != lastName && NULL != at_pos){
        ln_size = strlen(*lastName);
        n_size = strlen(*name);

        do{
            tmp = (char*)malloc( sizeof(char) * ln_size + n_size );
            if( NULL != tmp ){
                strncpy(tmp, (*name), at_pos - (*name));
                strcpy(tmp + (at_pos - (*name)), *lastName);
                strcpy(tmp + (at_pos - (*name)) + ln_size, at_pos + 1);
                free(*name);
                *name = tmp;
                n_size += (ln_size - 1);
            } else {
                res = ORCM_ERR_OUT_OF_RESOURCE;
            }
        }while( NULL != (at_pos = strchr(*name, '@')) && ORCM_SUCCESS == res );
    } else if( NULL == lastName && NULL != at_pos ){
        opal_output(0,"ERROR: You can't use '@' if there isn't a parent name.");
        res = ORCM_ERR_BAD_PARAM;
    }

    return res;
}

int cfgi30_add_valid_noregex_host_port(char ***hosts, int **ports, char **host,
                                       int *port, char ***new_names,
                                       char is_node)
{
    int res = ORCM_SUCCESS;

    if( is_node && NULL != (*host)
                && 0 != strcasecmp((*new_names)[0], *host)
                && 0 != strcasecmp(*host, "@")
                && 0 != strcasecmp(*host, "localhost") ){
        opal_output(0, "ERROR: A node controller host name must be the same as"
                       " the parent node name (or '@' if name it's a regex).");
        res = ORCM_ERR_BAD_PARAM;
    } else if( NULL != (*host) && 0 != strcasecmp(*host, "@") ){
        res = cfgi30_add_uniq_host_port(hosts, ports, host, port);
    }

    return res;
}

int cfgi30_add_valid_regex_hosts_ports(char ***hosts, int **ports, char **host,
                                       int *port, char ***new_names,
                                       char is_node, char is_regex,
                                       opal_list_t **next_junctions)
{
    int iterator = 0;
    int iterator2 = 0;
    int res = ORCM_SUCCESS;

    for( iterator = 0; NULL != (*new_names)[iterator] && ORCM_SUCCESS == res;
         iterator++ ){

        if( NULL != (*host) && 0 == strcasecmp(*host, "@") ){
            res = cfgi30_add_uniq_host_port(hosts, ports,
                        (*new_names) + iterator, port);
        }

        if( ORCM_SUCCESS == res ){
            for( iterator2 = 0; NULL != next_junctions[iterator2]
                                && ORCM_SUCCESS == res; iterator2++ ){
                res = cfgi30_check_junction_hosts_ports(
                            next_junctions[iterator2], hosts, ports,
                            (*new_names) + iterator );
            }
        }
    }

    return res;
}

int cfgi30_add_uniq_host_port(char ***hosts, int **ports, char **host, int *port)
{
    int res = ORCM_SUCCESS;

    if( !cfgi30_exists_host_port(hosts, ports, host, port) ){
        res = cfgi30_add_host_port(hosts, ports, host, port);
    } else {
        res = ORCM_ERR_BAD_PARAM;
        opal_output(0, "ERROR: Duplicate (s)host-port pair found: "
                       "port=%d host=%s", *port, *host);
    }

    return res;
}

int cfgi30_add_host_port(char ***hosts, int **ports, char **host, int *port)
{
    int res = ORCM_SUCCESS;
    int hosts_size = opal_argv_count(*hosts);
    char *tmp = strdup(*host);

    (*hosts) = (char**)realloc( (*hosts), sizeof(char*) * (hosts_size + 2) );
    (*ports) = (int*)realloc( (*ports), sizeof(int) * (hosts_size + 1) );

    if( NULL != *hosts && NULL != *ports && NULL != tmp ){
        (*hosts)[hosts_size] = tmp;
        (*ports)[hosts_size] = *port;
        (*hosts)[hosts_size + 1] = NULL;
    } else {
        res = ORCM_ERR_OUT_OF_RESOURCE;
        SAFEFREE(tmp);
    }

    return res;
}

char cfgi30_exists_host_port(char ***hosts, int **ports, char **host,
                             int *port)
{
    char exists = 0;
    int iterator = 0;

    for( iterator = 0; NULL != (*hosts)[iterator]; iterator++ ){
        if( 0 == strcasecmp( *host, (*hosts)[iterator] )
            && (*port) == (*ports)[iterator] ){
            exists = 1;
            break;
        }
    }

    return exists;
}

void cfgi30_free_string_array(char ***array)
{
    int iterator = 0;

    if( NULL != *array ){
        for( iterator = 0; NULL != (*array)[iterator]; iterator++ ){
            SAFEFREE((*array)[iterator]);
        }

        SAFEFREE(*array);
        *array = NULL;
    }
}

