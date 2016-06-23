/*
 * Copyright (c) 2014-2016  Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/query.h"

/**
 * @brief Array to correlate query arg types with their errors.
 *        This array must be closely related to the "orcm_db_qry_arg_types"
 *        enum on "db.h" of the DB framework. Relates each arg type
 *        to an error tag on "help-octl.txt.".
 */
char* orcm_db_qry_arg_types_error[] = {
    "",
    "no-date",
    "no-float",
    "no-integer",
    "no-interval",
    "no-ocomma_list",
    "no-string"
};

octl_query_cmd_t query_cmd[] = {
    { "query_event_data", "query-event-data", 'M',
        { ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_END } },
    { "query_event_snsr_data", "query-event-sensor-data", 'M',
        { ORCM_DB_QRY_INTEGER, ORCM_DB_QRY_INTERVAL|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_OCOMMA_LIST|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_END } },
    { "query_idle", "query-idle", 'S',
        { ORCM_DB_QRY_INTERVAL|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_END } },
    { "query_log", "query-log", 'M',
        { ORCM_DB_QRY_STRING, ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_OCOMMA_LIST|ORCM_DB_QRY_OPTIONAL, ORCM_DB_QRY_END } },
    { "query_node_status", "query-node-status", 'M',
        { ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_END } },
    { "query_sensor_history", "query-history", 'M',
        { ORCM_DB_QRY_OCOMMA_LIST|ORCM_DB_QRY_NULL,
          ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_STRING|ORCM_DB_QRY_NULL,
          ORCM_DB_QRY_STRING|ORCM_DB_QRY_NULL,
          ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_END } },
    { "query_sensor_history", "query-sensor", 'M',
        { ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_DATE|ORCM_DB_QRY_OPTIONAL, ORCM_DB_QRY_STRING,
          ORCM_DB_QRY_STRING, ORCM_DB_QRY_OCOMMA_LIST|ORCM_DB_QRY_OPTIONAL,
          ORCM_DB_QRY_END } },
    { "query_snsr_get_inventory", "sensor-get-inventory", 'M',
        { ORCM_DB_QRY_OCOMMA_LIST, ORCM_DB_QRY_END } }
};

int orcm_octl_query_func(query_func_names func_name, int argc, char **argv)
{
    int rc = ORCM_SUCCESS;
    opal_list_t *query_func_args = NULL;

    rc = query_args_list(func_name, &query_func_args, argc, argv);
    if( ORCM_SUCCESS == rc ) {
        rc = query_args_add_limit(OCTL_QUERY_DFLT_LIMIT, &query_func_args);
        if( ORCM_SUCCESS == rc ) {
            rc = query(func_name, query_func_args, OCTL_QUERY_DFLT_LIMIT);
        }
    } else if( ORCM_ERR_BAD_PARAM == rc) {
        orcm_octl_usage(query_cmd[func_name].cmd_usage_info, INVALID_USG);
    }
    query_custom_opal_list_free(&query_func_args);

    return rc;
}

int query_args_list(query_func_names func_name, opal_list_t **args, int argc,
                    char **argv)
{
    int argc_remain = argc;
    char **argv_remain = argv;
    int iterator = 0;
    int rc = ORCM_SUCCESS;
    char *aux_buff = NULL;
    orcm_db_qry_arg_types pure_type = ORCM_DB_QRY_END;

    *args = OBJ_NEW(opal_list_t);
    if( NULL != (*args) ) {
        for(; ORCM_DB_QRY_END != query_cmd[func_name].args[iterator]
                  && ORCM_SUCCESS == rc; iterator++ ) {

            if( 0 != (query_cmd[func_name].args[iterator] & ORCM_DB_QRY_NULL) ) {
                pure_type = ORCM_DB_QRY_NULL;
            } else {
                pure_type = query_cmd[func_name].args[iterator]
                           & (ORCM_DB_QRY_OPTIONAL - 1);
            }

            switch( pure_type ) {
                case ORCM_DB_QRY_DATE:
                    rc = query_validate_date(&argc_remain, argv_remain,
                                             &aux_buff);
                    break;
                case ORCM_DB_QRY_FLOAT:
                    rc = query_validate_float(&argc_remain, argv_remain, &aux_buff);
                    break;
                case ORCM_DB_QRY_INTEGER:
                    rc = query_validate_integer(&argc_remain, argv_remain, &aux_buff);
                    break;
                case ORCM_DB_QRY_INTERVAL:
                    rc = query_validate_bef_aft(&argc_remain, argv_remain,
                                                query_cmd[func_name].interval_dflt,
                                                &aux_buff);
                    break;
                case ORCM_DB_QRY_OCOMMA_LIST:
                    rc = query_validate_string(&argc_remain, argv_remain, &aux_buff);
                    if( ORCM_SUCCESS == rc) {
                        rc = query_comma_list_expand(&aux_buff);
                    }
                    break;
                case ORCM_DB_QRY_STRING:
                    rc = query_validate_string(&argc_remain, argv_remain, &aux_buff);
                    break;
                case ORCM_DB_QRY_NULL:
                    break;
                default:
                    orcm_octl_error("qry-unk-db-data-type");
                    rc = ORCM_ERR_BAD_PARAM;
                    break;
            }

            if( ORCM_ERR_OUT_OF_RESOURCE != rc ) {
                query_args_post_val(&rc, query_cmd[func_name].args[iterator],
                                   aux_buff, *argv_remain, args);
            }

            SAFEFREE(aux_buff);
            argv_remain = argv + (sizeof(char) * (argc - argc_remain));
        }

        if( 0 < argc_remain && ORCM_SUCCESS == rc ) {
            orcm_octl_info("query-extra-args", *argv_remain,
                           argc - argc_remain + 1);
            orcm_octl_info(query_cmd[func_name].cmd_usage_info);
        }
    } else {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
    }

    return rc;
}

int query_args_add_limit(int limit, opal_list_t **args)
{
    int rc = ORCM_SUCCESS;
    int length = 0;
    char *str_limit = NULL;

    if( limit > 0 ) {
        length = log10( limit ) + 2;
        str_limit = (char*)malloc(sizeof(char) * length);
        if( NULL == str_limit ) {
            rc = ORCM_ERR_OUT_OF_RESOURCE;
        } else {
            sprintf(str_limit, "%d", limit);
        }
    } else {
        rc = ORCM_ERR_BAD_PARAM;
    }

    if( ORCM_ERR_OUT_OF_RESOURCE != rc ) {
        query_args_post_val(&rc, ORCM_DB_QRY_INTEGER|ORCM_DB_QRY_OPTIONAL,
                                   str_limit, str_limit, args);
        SAFEFREE(str_limit);
    }

    return rc;
}

int query_args_post_val(int *rc, orcm_db_qry_arg_types type, char *arg,
                        char *original_arg, opal_list_t **args)
{
    opal_value_t *opal_arg = NULL;
    orcm_db_qry_arg_types pure_type = type & (ORCM_DB_QRY_OPTIONAL - 1);

    opal_arg = OBJ_NEW(opal_value_t);
    if( NULL != opal_arg ) {
        switch( *rc ) {
            case ORCM_SUCCESS:
                opal_arg->type = pure_type;

                if( 0 != (type & ORCM_DB_QRY_NULL) ) {
                    opal_arg->data.string = NULL;
                } else {
                    opal_arg->data.string = strdup(arg);
                    if( NULL == opal_arg->data.string ) {
                        *rc = ORCM_ERR_OUT_OF_RESOURCE;
                    }
                }
                break;
            case ORCM_ERR_BAD_PARAM:
                if( 0 != (type & ORCM_DB_QRY_OPTIONAL) ) {
                    opal_arg->type = pure_type;
                    opal_arg->data.string = NULL;
                    *rc = ORCM_SUCCESS;
                } else {
                    orcm_octl_error(orcm_db_qry_arg_types_error[pure_type],
                                    original_arg);
                }
                break;
        }

        opal_list_append(*args, &opal_arg->super);
    } else {
        *rc = ORCM_ERR_OUT_OF_RESOURCE;
    }

    return *rc;
}

int query_validate_date(int *argc, char **argv, char **date)
{
    regmatch_t ts_matches[2];
    struct tm *tm_date;
    time_t t_date;
    int res = ORCM_ERR_BAD_PARAM;

    *date = strdup("0000-00-00 00:00:00");
    if( NULL != (*date) ){
        res = query_validate_with_regex(ts_matches, 2, REG_EXTENDED, argv[0],
                "^[[:digit:]]{4}-[0-1][[:digit:]]-[0-3][[:digit:]]"
                "([[:space:]][0-2][[:digit:]]:[0-5][[:digit:]]:[0-5][[:digit:]])?$");
        if( ORCM_SUCCESS == res ) {
            strncpy((*date), argv[0], ts_matches[0].rm_eo - ts_matches[0].rm_so);
            (*argc)--;
            argv += sizeof(char);
        } else if( ORCM_ERR_BAD_PARAM == res ){
            setlocale(LC_TIME, "UTC");
            time( &t_date );
            if( NULL != (tm_date = localtime( &t_date )) ) {
                tm_date->tm_isdst = -1;
                strftime((*date), 19, "%Y-%m-%d 00:00:00", tm_date);
            } else {
                res = ORCM_ERR_OUT_OF_RESOURCE;
            }
        }

        if( ORCM_ERROR != res && ORCM_ERR_OUT_OF_RESOURCE != res
            && 0 < (*argc)
            && 0 >= (ts_matches[1].rm_eo - ts_matches[1].rm_so) ) {

            if( ORCM_SUCCESS
                == query_validate_with_regex(NULL, 0, REG_EXTENDED, argv[0],
                        "^[0-2][[:digit:]]:[0-5][[:digit:]]:[0-5][[:digit:]]$") ) {
                strcpy((*date) + sizeof(char) * 11, *argv);
                (*argc)--;
                res = ORCM_SUCCESS;
            }
        }
    } else {
        res = ORCM_ERR_OUT_OF_RESOURCE;
    }

    return res;
}

int query_validate_interval(int *argc, char **argv)
{
    int res = ORCM_ERR_BAD_PARAM;

    res = query_validate_with_regex(NULL, 0, REG_EXTENDED, argv[0],
            "^[+-]?[[:digit:]]{2,}:[0-5][[:digit:]]:[0-5][[:digit:]]$");

    if( ORCM_SUCCESS == res ){
        (*argc)--;
    }

    return res;
}

int query_validate_integer(int *argc, char **argv, char **data)
{
    int res = ORCM_ERR_BAD_PARAM;

    res = query_validate_with_regex(NULL, 0, REG_EXTENDED, argv[0],
                                    "^[+-]?[[:digit:]]+$");
    if( ORCM_SUCCESS == res ) {
        *data = strdup(argv[0]);
        if( NULL != (*data) ) {
            (*argc)--;
        } else {
            res = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return res;
}

int query_validate_float(int *argc, char **argv, char **data)
{
    int res = ORCM_ERR_BAD_PARAM;

    res = query_validate_with_regex(NULL, 0, REG_EXTENDED, argv[0],
            "^[+-]?(([[:digit:]]+)|([[:digit:]]*\\.[[:digit:]]+))$");
    if( ORCM_SUCCESS == res ) {
        *data = strdup(argv[0]);
        if( NULL != (*data) ) {
            (*argc)--;
        } else {
            res = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return res;
}

int query_validate_string(int *argc, char **argv, char **data)
{
    int res = ORCM_ERR_BAD_PARAM;

    res = query_validate_with_regex(NULL, 0, REG_EXTENDED, argv[0], "^.+$");
    if( ORCM_SUCCESS == res ) {
        *data = strdup(argv[0]);
        if( NULL != (*data) ) {
            (*argc)--;
        } else {
            res = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return res;
}

int query_validate_bef_aft(int *argc, char **argv, char default_unit,
                           char **data)
{
    regmatch_t reg_matches[3];
    char before = 0;
    int argc_bef_aft = 0;
    int res = ORCM_ERR_BAD_PARAM;

    res = query_validate_with_regex(reg_matches, 3, REG_EXTENDED, argv[0],
                                    "^(before)|(after)$");
    if( ORCM_SUCCESS == res ){
        before = ( -1 < reg_matches[1].rm_so ) ? 1:0;
        argv += sizeof(char);
        argc_bef_aft++;
        (*argc)--;
    }

    if( ORCM_ERROR != res
        && ORCM_ERR_BAD_PARAM
           == (res = query_validate_bef_aft_interval(argc, argv,
                                                     default_unit,
                                                     before, data)) ) {
        (*argc) += argc_bef_aft;
    }

    return res;
}

int query_validate_bef_aft_interval(int *argc, char **argv, char default_unit,
                                    char before, char **interval)
{
    regmatch_t reg_matches[6];
    int res = ORCM_SUCCESS;
    double quantity = 0;

    res = query_validate_with_regex(reg_matches, 6, REG_EXTENDED, argv[0],
            "^([+-])?(([[:digit:]]+)|([[:digit:]]*\\.[[:digit:]]+))([HMS])?$");
    if( ORCM_SUCCESS == res ) {
        (*argc)--;
        if( -1 < reg_matches[1].rm_so
            && '-' == argv[0][reg_matches[1].rm_so] ) {
            before = !before;
        }

        *interval = strndup( argv[0] + reg_matches[2].rm_so,
                             reg_matches[2].rm_eo - reg_matches[2].rm_so);
        if( NULL != (*interval) ) {
            quantity = atof(*interval) * pow(-1,before);
            SAFEFREE(*interval);

            if( -1 < reg_matches[5].rm_so ) {
                default_unit = argv[0][reg_matches[5].rm_so];
            }
            res = query_double_to_interval( quantity, default_unit,
                                            interval);
        } else {
            res = ORCM_ERR_OUT_OF_RESOURCE;
        }
    } else if( ORCM_ERR_BAD_PARAM == res
               && ORCM_SUCCESS
                  == (res = query_validate_interval(argc, argv)) ) {

        if( before ) {
            if( '-' == argv[0][0] || '+' == argv[0][0] ) {
                *interval = strdup( *argv + sizeof(char) );
            } else {
                *interval = strdup( argv[0] );
            }

            if( '-' != argv[0][0]  && NULL != (*interval) ) {
                *interval = (char*)realloc( *interval, strlen(*interval)
                                                       + sizeof(char) * 2 );
                if( NULL != (*interval) ) {
                    strcpy( *interval + sizeof(char), (*interval) );
                    (*interval)[0] = '-';
                }
            }
        } else {
            *interval = strdup( argv[0] );
        }

        if( NULL == (*interval) ) {
            res = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return res;
}

int query_validate_with_regex(regmatch_t *reg_matches, size_t nmatches,
                              int eflags, char* str_eval,
                              const char* str_regex) {
    regex_t regex_comp;
    int regex_res;
    int res = ORCM_ERR_BAD_PARAM;

    if( NULL != str_eval ){
        regcomp(&regex_comp, str_regex, eflags);

        regex_res = regexec(&regex_comp, str_eval, nmatches, reg_matches, 0);
        if( !regex_res ){
            res = ORCM_SUCCESS;
        } else if( REG_NOMATCH != regex_res ){
            orcm_octl_error("no-regex");
            res = ORCM_ERROR;
        }
        regfree(&regex_comp);
    }

    return res;
}

int query_comma_list_expand(char **comma_list)
{
    typedef int (*oregex_logroup_expander_fn_t)(char *expandable,
                                                char ***results);

    int rc = ORCM_SUCCESS;
    int regex_res = 0;
    regex_t regex_comp_oregex;
    regex_t regex_comp_logroup;
    regmatch_t list_matches[2];
    char *expandable = NULL;
    char **expanded = NULL;
    oregex_logroup_expander_fn_t expander;
    int list_length = 0;
    int str_pos = 0;


    if( NULL != *comma_list){
        regcomp(&regex_comp_oregex,
                "([^[,]+\\[[[:digit:]]+:[[:digit:]]+-[[:digit:]]+\\]),?",
                REG_EXTENDED);
        regcomp(&regex_comp_logroup, "(\\$[^,]+),?", REG_EXTENDED);

        list_length = strlen(*comma_list);

        while( ORCM_SUCCESS == rc && !regex_res ) {

            if( !(regex_res = regexec(&regex_comp_oregex,
                                      (*comma_list) + str_pos, 2,
                                      list_matches,0)) ) {
                expander = orte_regex_extract_node_names;
            }
            else if( REG_NOMATCH == regex_res
                     && !(regex_res = regexec(&regex_comp_logroup,
                                              (*comma_list) + str_pos, 2,
                                              list_matches,0)) ) {
                expander = orcm_logical_group_parse_array_string;
            }

            if ( !regex_res ) {
                expandable = strndup( (*comma_list) + str_pos
                                          + list_matches[1].rm_so,
                                      (int)(list_matches[1].rm_eo
                                          - list_matches[1].rm_so));
                if( NULL != expandable ) {
                    memmove( (void *)((*comma_list) + str_pos
                                         + list_matches[0].rm_so),
                             (void *)((*comma_list) + str_pos
                                         + list_matches[0].rm_eo),
                             list_length - str_pos - list_matches[0].rm_eo + 1);
                    list_length = strlen(*comma_list);
                    str_pos += list_matches[0].rm_so;
                    if( list_length && (*comma_list)[list_length - 1] == ',' ) {
                        (*comma_list)[list_length - 1] = '\x0';
                        list_length--;
                    }

                    if( ORCM_SUCCESS
                            == (rc = expander(expandable, &expanded)) ) {
                        rc = query_comma_list_add_unique_elements(comma_list,
                                                                  &expanded,
                                                                  &list_length);
                    }
                    SAFEFREE(expandable);
                } else {
                    rc = ORCM_ERR_OUT_OF_RESOURCE;
                }
            } else if( REG_NOMATCH != regex_res ) {
                orcm_octl_error("no-regex");
                rc = ORCM_ERROR;
            }
        }
        regfree(&regex_comp_oregex);
        regfree(&regex_comp_logroup);
    }

    return rc;
}

int query_comma_list_add_unique_elements(char **comma_list, char ***elements,
                                         int *list_length)
{
    int rc = ORCM_SUCCESS;
    int iterator = 0;
    int element_length = 0;
    int regex_res = 0;
    regex_t regex_comp;
    char *element_regex = NULL;

    if( NULL != *elements ) {
        for( iterator=0; (*elements)[iterator] != NULL
                         && ORCM_SUCCESS == rc;
             iterator++ ){
            element_regex = strdup((*elements)[iterator]);

            if( NULL != element_regex ) {
                rc = query_comma_list_element_to_regex( &element_regex,
                                                        strlen(element_regex) );
                if( ORCM_SUCCESS == rc ) {
                    regcomp(&regex_comp, element_regex, REG_EXTENDED);

                    regex_res = regexec(&regex_comp, *comma_list, 0, NULL, 0);
                    if( REG_NOMATCH == regex_res ) {
                        element_length = strlen((*elements)[iterator]);
                        if( 0 != (*list_length) ) {
                            (*comma_list)[*list_length] = ',';
                            (*list_length)++;
                        }

                        *comma_list = (char *)realloc( *comma_list,
                                                       ((*list_length)
                                                           + element_length + 1)
                                                       * sizeof(char) );
                        if( NULL != *comma_list) {
                            strcpy( (*comma_list) + (*list_length),
                                    (*elements)[iterator] );
                            (*list_length) += element_length;
                        } else {
                            rc = ORCM_ERR_OUT_OF_RESOURCE;
                        }
                    } else if( regex_res ) {
                        orcm_octl_error("no-regex");
                        rc = ORCM_ERROR;
                    }

                    regfree(&regex_comp);
                    SAFEFREE((*elements)[iterator]);
                }
            } else {
                rc = ORCM_ERR_OUT_OF_RESOURCE;
            }
            SAFEFREE(element_regex);
        }
    }

    return rc;
}

int query_comma_list_element_to_regex(char **str, int act_length)
{
    int rc;

    if( ORCM_SUCCESS == (rc = query_str_to_regex(str, act_length)) ) {
        act_length = strlen(*str);

        *str = (char *)realloc( *str, act_length + 11 );
        if( NULL != *str ) {
            memmove( (void *)((*str) + 5), (void *)(*str), act_length );
            memmove( (void *)(*str), "(^|,)", 5 );
            strcpy( (*str) + act_length + 5, "(,|$)" );
        } else {
            rc = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return rc;
}

int query_str_to_regex(char **str, int act_length)
{
    int rc = ORCM_SUCCESS;
    int str_pos = 0;
    int regex_res = 0;
    regex_t regex_comp;
    regmatch_t str_matches[1];

    regcomp(&regex_comp, "(\\\\|\\^|\\$|\\.|\\]|[[(){}+*?|-])", REG_EXTENDED);

    while( ORCM_SUCCESS == rc && !regex_res) {

        regex_res = regexec(&regex_comp, (*str) + str_pos, 1, str_matches,0);
        if( !regex_res ) {
            *str = (char *)realloc( *str, act_length + 2 );

            if( NULL != (*str) ) {
                memmove( (void *)((*str) + str_pos + str_matches[0].rm_eo),
                         (void *)((*str) + str_pos + str_matches[0].rm_so),
                         act_length - str_pos - str_matches[0].rm_so + 1);
                (*str)[str_pos + str_matches[0].rm_so] = '\\';
                str_pos += str_matches[0].rm_eo + 1;
                act_length += 2;
            } else {
                rc = ORCM_ERR_OUT_OF_RESOURCE;
            }
        } else if( REG_NOMATCH != regex_res ) {
            orcm_octl_error("no-regex");
            rc = ORCM_ERROR;
        }
    }
    regfree(&regex_comp);

    return rc;
}

int query_double_to_interval(double quantity, const char units, char **interval)
{
    int rc = ORCM_SUCCESS;
    double hours = 0, minutes = 0, tminutes = 0;
    int seconds = 0, tseconds = 0;
    int length = 0;

    switch( units ) {
        case 'S':
            seconds = tseconds = fabs(quantity);
            break;
        case 'M':
            minutes = tminutes = fabs(quantity);
            break;
        case 'H':
            hours = fabs(quantity);
            break;
        default:
            orcm_octl_error("no-interval-unit", units);
            rc = ORCM_ERR_BAD_PARAM;
            break;
    }

    if( ORCM_SUCCESS == rc ) {
        minutes += modf(hours, &hours) * 60;
        tseconds = (seconds += modf(minutes, &minutes) * 60);
        tminutes = minutes;

        seconds = seconds % 60;
        tminutes = (minutes += (int)(tseconds/60));
        minutes = (int)minutes % 60;
        hours += (int)(tminutes/60);

        length = log10( abs( (hours)?hours:1 ) ) + 2;
        *interval = (char*)malloc(sizeof(char) * (length + 7));
        if( NULL != (*interval) ) {
            sprintf(*interval, (0 > quantity)?"-%02d:%02d:%02d":
                    "%02d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
        } else {
            rc = ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return rc;
}

int open_database()
{
    bool valid_component;
    query_data.dbhandle = -1;
    query_data.active = true;
    orcm_db.open(NULL, NULL, open_callback, &query_data);
    ORTE_WAIT_FOR_COMPLETION(query_data.active);
    if (ORCM_SUCCESS != query_data.status) {
        return ORCM_ERROR;
    }
    /* Check if has valid component */
    valid_component = is_valid_db_component_selected();
    if (!valid_component) {
        return ORCM_ERR_NOT_FOUND;
    }
    return ORCM_SUCCESS;
}

bool is_valid_db_component_selected() {
    orcm_db_handle_t *hdl;
    hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles,
                                                         query_data.dbhandle);
    if (NULL != hdl) {
        if (strcmp("print", hdl->component->base_version.mca_component_name)) {
            return true;
        }
    }
    return false;
}

int close_database()
{
    int rc;
    rc = orcm_db.close_result_set(query_data.dbhandle, query_data.session_handle);
    if (ORCM_SUCCESS != rc) {
      return rc;
    }
    query_data.active = true;
    orcm_db.close(query_data.dbhandle, close_callback, &query_data);
    ORTE_WAIT_FOR_COMPLETION(query_data.active);
    if (ORCM_SUCCESS != query_data.status) {
      return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

int fetch_data_from_db(query_func_names db_func_name, opal_list_t *filters)
{
    opal_list_t *results = OBJ_NEW(opal_list_t);
    if (NULL == results) {
        return ORCM_ERROR;
    }
    query_data.active = true;
    orcm_db.fetch_function(query_data.dbhandle, query_cmd[db_func_name].sql_func_name, filters, results, fetch_callback, &query_data);
    ORTE_WAIT_FOR_COMPLETION(query_data.active);

    if (ORCM_SUCCESS != query_data.status || -1 == query_data.session_handle) {
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

int get_query_num_rows()
{
    int num_rows = 0;
    int rc = ORCM_SUCCESS;
    rc = orcm_db.get_num_rows(query_data.dbhandle, query_data.session_handle, &num_rows);
    if (ORCM_SUCCESS != rc) {
      num_rows = -1;
    }
    return num_rows;
}

int query_get_next_row(opal_list_t *row)
{
    int rc;
    if (NULL == row) {
      return ORCM_ERROR;
    }
    rc = orcm_db.get_next_row(query_data.dbhandle, query_data.session_handle, row);
    if (ORCM_SUCCESS != rc) {
      return rc;
    }

    return ORCM_SUCCESS;
}

int print_row(opal_list_t *row, const char *separator, bool is_header)
{
    /* Heads up! the incoming data may not be only string
     * and this function need to be changed in the future.
     */
    opal_value_t *item;
    unsigned int item_count = 0;
    if (NULL == row) {
      return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(item, row, opal_value_t) {
      if (OPAL_STRING == item->type) {
        item_count++;
        if (is_header) {
            printf("%s", item->key);
        } else {
            printf("%s", item->data.string);
        }
        if (row->opal_list_length > item_count) {
          printf("%s", separator);
        }
      }
    }
    printf("\n");
    return ORCM_SUCCESS;
}

int print_all_query_results(time_t *query_time, int query_limit)
{
    int returned_rows;
    int i;
    opal_list_t *row;
    int rc;
    bool is_header = true;
    returned_rows = get_query_num_rows();
    if (-1 == returned_rows) {
        opal_output(0, "Failed to get amount of returned rows");
        return ORCM_ERROR;
    }

    /* Print all results one by one, not loading all the results in memory */
    for (i = 0; i < returned_rows; i++) {
        row = OBJ_NEW(opal_list_t);
        if (NULL == row) {
            opal_output(0, "Error creating opal_list_t");
            return ORCM_ERROR;
        }
        rc = query_get_next_row(row);
        if (ORCM_SUCCESS != rc) {
            break;
        }

        if( is_header ) {
            rc = print_row(row, ",", is_header);
            is_header = false;
        }
        rc = print_row(row, ",", is_header);
        SAFE_RELEASE(row);
    }
    printf("\n");
    orcm_octl_info("rows-found", returned_rows, ((double)*query_time));

    if( query_limit ) {
        orcm_octl_info("rows-limit", query_limit);
    }

    return returned_rows;
}

int query(query_func_names db_func, opal_list_t *filters, int query_limit)
{
    int rc;
    int returned_rows;
    time_t query_time;

    rc = open_database();
    if (ORCM_ERROR == rc) {
        orcm_octl_error("connection-db-fail");
        return rc;
    } else if (ORCM_ERR_NOT_FOUND == rc) {
        orcm_octl_error("bad-db-component");
        return rc;
    }

    query_time = time(NULL);
    rc = fetch_data_from_db(db_func, filters);
    query_time = time(NULL) - query_time;
    if (ORCM_SUCCESS != rc) {
        orcm_octl_error("fetch-db-fail");
        return rc;
    }

    returned_rows = print_all_query_results(&query_time, query_limit);

    rc = close_database();
    if (ORCM_SUCCESS != rc) {
        orcm_octl_error("disconnect-db-fail");
    }
    return rc;
}

void query_custom_opal_list_free(opal_list_t **opal_list)
{
    opal_value_t *it = NULL;

    if( NULL != *opal_list ) {
        while( NULL != (it = (opal_value_t *)opal_list_remove_first(*opal_list)) ) {
            if( NULL != it->data.string ) {
                free(it->data.string);
                it->data.string = NULL;
            }

            free(it);
        }
        OBJ_RELEASE(*opal_list);
    }
}
