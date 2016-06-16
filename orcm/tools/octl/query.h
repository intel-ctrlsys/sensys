/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_OCTL_QUERY_H
#define ORCM_OCTL_QUERY_H

#include "orcm/util/logical_group.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/mca/db/db.h"
#include "orcm/tools/octl/common.h"
#include "orcm/util/utils.h"
#include <locale.h>
#include <math.h>
#include <regex.h>
#include <time.h>

BEGIN_C_DECLS

#define OCTL_QUERY_MAX_ARGS 11
#define OCTL_QUERY_DFLT_LIMIT 50

typedef struct {
    int dbhandle;
    int session_handle;
    int number_of_rows;
    int status;
    bool active;
} fetch_cb_data;


/**
 * @brief Structure that contains a query function information.
 *        Structure that holds all the arguments that defines
 *        a query function allowing to run it and validate it.
 */
typedef struct {
    char *sql_func_name; /**< Name of the query function to exec from the DBM*/
    char *cmd_usage_info;     /**< Tag to extract the query usage and info from
                                   help-octl.txt */
    char interval_dflt;  /**< Some queries use intervals, this defines the
                              default unit ('H','M' or 'S') */
    orcm_db_qry_arg_types args[OCTL_QUERY_MAX_ARGS]; /**< Array that contains
                              the expected type of the query arguments */
} octl_query_cmd_t;
OBJ_CLASS_DECLARATION(octl_query_cmd_t);

extern octl_query_cmd_t query_cmd[];
extern char* orcm_db_qry_arg_types_error[];
extern orcm_db_API_module_t orcm_db;

extern void open_callback(int handle, int status, opal_list_t *props,
                          opal_list_t *ret, void *cbdata);
extern void fetch_callback(int handle, int status, opal_list_t *props,
                           opal_list_t *ret, void *cbdata);
extern void close_callback(int handle, int status, opal_list_t *props,
                           opal_list_t *ret, void *cbdata);

fetch_cb_data query_data;

/**
 * @brief Adds a limit of results to a query function.
 *        Currently query commands does not support to specify
 *        a limit of results. This function allows to add a limit
 *        to the argument list of a query function.
 *
 * @param limit Quantity of results to obtain from query.
 *
 * @param args Current list of arguments to pass to the query function.
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_args_add_limit(int limit, opal_list_t **args);

/**
 * @brief Creates and validates an argument list for a query function.
 *        Creates and validates an argument list from the arguments
 *        provided to a query command. The argument list will be specific
 *        to a query function depending of the provided function name.
 *
 * @param func_name Function name for which the arguments will be validated
 *                  for.
 *
 * @param args Reference to an opal_list_t that will contain the validated
 *             arguments for the query function.
 *
 * @param argc Current count of arguments to validate in the executed query
 *             command.
 *
 * @param argv Current array of arguments to validate in the executed query
 *             command.
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_args_list(query_func_names func_name, opal_list_t **args, int argc,
                    char **argv);

/**
 * @brief Attaches a valid argument to a query function argument list.
 *        Verifies if the validated argument was or wasn't valid and, if it
 *        wasn't, it checks if it was valid anyway because it was optional
 *        or intended to be null. If everything was fine, the argument will be
 *        attached to the query function argument list.
 *
 * @param rc Reference to a return code that indicates if the previous
 *           argument validation was successful (ORCM_SUCCESS) or not.
 *
 * @param type Current expected type of the previously validated argument.
 *
 * @param arg Current content of the validated argument.
 *
 * @param original_arg Reference of the original validated argument.
 *                     Only used if there is the need to report an error.
 *
 * @param args Reference to the argument list of the query function.
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_args_post_val(int *rc, orcm_db_qry_arg_types type, char *arg,
                        char *original_arg, opal_list_t **args);

/**
 * @brief Adds elements to an orcm comma list if they weren't already in there.
 *        Adds several elements (elements) to a orcm comma separated list
 *        (comma_list) only if they weren't already in the list.
 *
 *        Example
 *        Having the following orcm comma list
 *            "node01,node03,node05"
 *        And assuming that "elements" contains
 *            "node01,node02,node03,node04,node05"
 *        Result will be the following
 *            "node01,node03,node05,node02,node04"
 *
 * @param comma_list Reference to a comma separated string.
 *
 * @param elements Reference to an array of elements (strings) to add to
 *                 the list.
 *
 * @param list_length Reference to an integer that contains the actual length
 *                    of the list. Equivalent to strlen(*comma_list).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_comma_list_add_unique_elements(char **comma_list, char ***elements,
                                         int *list_length);

/**
 * @brief Escapes and adds patterns to a comma list element making it a regex.
 *        Converts an orcm comma list element to a regular expression so,
 *        it can be searched for coincidences. Result is returned into
 *        the same element (str).
 *
 *        Example
 *        Having the following orcm comma list element
 *            "node01.domain-info"
 *        Result will be the following
 *            "(^|,)node01\.domain\-info(,|$)"
 *
 * @param comma_list Reference to a comma separated string.
 *
 * @param elements Reference to an array of elements (strings) to add to
 *                 the list.
 *
 * @param list_length Reference to an integer that contains the actual length
 *                    of the list. Equivalent to strlen(*comma_list).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_comma_list_element_to_regex(char **str, int act_length);

/**
 * @brief Expands logical groups and regexs that are in an orcm comma list.
 *        Expands an orcm comma separated list into the same comma separated
 *        list (comma_list).
 *        Regex like "something[#:#-#]" will be expanded.
 *        Logical groups like "$group_name" also will be expanded.
 *        During expansion no repeated elements will be inserted.
 *
 *        Example
 *        Having the following orcm comma list
 *            "node01,$test_group,node[2:1-5]"
 *        And assuming that "$test_group" contains
 *            "node03,node05"
 *        Result will be the following
 *            "node01,node03,node05,node02,node04"
 *
 * @param comma_list Reference to the comma separated string that will
 *                   be expanded into this same parameter.
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_comma_list_expand(char **comma_list);

/**
 * @brief opal_lists items couldn't be free in an appropriate way because the
 *        pointers of their structures are not NULL initialized. Having that
 *        in mind, their destructors never know if they really have memory
 *        reserved or not and never try to destruct them causing memory leaks.
 *        This function overrides that problem but only for opal_value_t items.
 *
 * @param opal_list OPAL LIST to be free together with its items.
 *
 */
void query_custom_opal_list_free(opal_list_t **opal_list);

/**
 * @brief Checks if an argument has a "[before/after] interval" syntax.
 *        Verifies if the argument passed in the first (and maybe second)
 *        position of the argv array it's a before/after argument followed
 *        by an interval. Returns the interval into the "data" argument under
 *        the format "HH:MM:SS"
 *        "argc" will decrease by 1 or 2 depending of what's found.
 *
 *        Interval could be in the format "HH:MM:SS" or like a float
 *        followed by a "H", "M", or "S" were
 *            "H" = Hours, "M" = Minutes, "S" = Seconds
 *
 *        Example1 (argc decreases by 2)
 *            argv[0] = "before"
 *            argv[1] = "-1.5H"
 *            data = "01:30:00"
 *
 *        Example2 (argc decreases by 1)
 *            argv[0] = "-00:30:00"
 *            data = "-00:30:00"
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that contained the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the arguments to validate starts on
 *             position 0.
 *
 * @param default_unit If no unit is passed as part of a numeric interval
 *                     then, this unit will be used. (H, M or S)
 *
 * @param data Reference to a string that will contain the result of
 *             transforming the interval in to the format "HH:MM:SS".
 *             (DYNAMICALLY RESERVED INSIDE, NEED TO FREE FROM OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_bef_aft(int *argc, char **argv, char default_unit,
                           char **data);

/**
 * @brief Checks if an argument has an "interval" syntax.
 *        Verifies if the argument passed in the first position of the argv
 *        array it's an interval. Returns the interval into the "interval"
 *        argument under the format "HH:MM:SS"
 *        "argc" will decrease by 1 if an interval it's found.
 *
 *        Interval could be in the format "HH:MM:SS" or like a float
 *        followed by a "H", "M", or "S" were
 *            "H" = Hours, "M" = Minutes, "S" = Seconds
 *
 *        Example1
 *            argv[0] = "-1.5H"
 *            before = 1
 *            data = "01:30:00"
 *
 *        Example2
 *            argv[0] = "-00:30:00"
 *            before = 0
 *            data = "-00:30:00"
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that contained the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's in
 *             position 0.
 *
 * @param default_unit If no unit is passed as part of a numeric interval
 *                     then, this unit will be used. (H, M or S)
 *
 * @param before If this flag is set it means that a backwards interval
 *               will be specified unless it gets affected by a "-" (minus)
 *               sign into the interval.
 *
 * @param interval Reference to a string that will contain the result of
 *                 transforming the interval in to the format "HH:MM:SS".
 *                 (DYNAMICALLY RESERVED INSIDE, NEED TO FREE FROM OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_bef_aft_interval(int *argc, char **argv, char default_unit,
                                    char before, char **interval);

/**
 * @brief Checks if an argument has a "date" syntax.
 *        Verifies if the argument passed in the first position of
 *        the argv array it's a date and returns it into "date"
 *        parameter as a single string.
 *        "argc" will decrease the number of positions in argv that
 *        contains date data.
 *
 *        Date can be in the following formats.
 *
 *            Split in two arguments
 *                argv[0] = "2016-05-05"
 *                argv[1] = "00:00:00"
 *                Returns -> "2016-05-05 00:00:00"
 *            Into a single argument
 *                argv[0] = "2016-05-05 00:00:00"
 *                Returns -> "2016-05-05 00:00:00"
 *            Just year, month and day "00:00:00" will be added
 *                argv[0] = "2016-05-05"
 *                Returns -> "2016-05-05 00:00:00"
 *            Just time of the day, current date will be added
 *            assuming today is "2016-05-05", result will be
 *                argv[0] = "12:12:12"
 *                Returns -> "2016-05-05 12:12:12"
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that containded the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's on
 *             position 0.
 *
 * @param date Reference to the string were the date will be
 *             returned. (DYNAMICALLY RESERVED INSIDE, NEED TO FREE FROM
 *             OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_date(int *argc, char **argv, char **date);

/**
 * @brief Checks if a provided string matches a provided regex.
 *        Just joins the regcomp and regexec commands into a single one to make
 *        easier/faster the query db data types validation when a straight
 *        validation is needed. Also was made to improve code coverage.
 *
 * @param reg_matches Array that will contain the matches information
 *                    if parenthesis were given in the regex.
 *
 * @param nmatches Number of matches to collect from the string. Must be
 *                 >= reg_matches array size.
 *
 * @param eflags Flags to be considered while evaluating the regex.
 *               Tipically will be REG_EXTENDED.
 *
 * @param str_eval String to be evaluated by the regex.
 *
 * @param str_regex String with the regex to evaluate.
 *
 * @return int Return code that indicates if str_eval matches str_regex
 *                    (ORCM_SUCCESS) or not (ORCM_ERR_BAD_PARAM) or, there
 *                    was an error on the regex (ORCM_ERROR).
 */
int query_validate_with_regex(regmatch_t *reg_matches, size_t nmatches,
                              int eflags, char* str_eval,
                              const char* str_regex);
/**
 * @brief Checks if an argument has a "float" syntax.
 *        Verifies if the argument passed in the first position of
 *        the argv array it's a float.
 *        "argc" will decrease by 1 if a float is found.
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that containded the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's on
 *             position 0.
 *
 * @param data Reference to a string that will contain the data
 *             contained in argv[0] if it was valid. (DYNAMICALLY
 *             RESERVED INSIDE, NEED TO FREE FROM OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_float(int *argc, char **argv, char **data);

/**
 * @brief Checks if an argument has an "integer" syntax.
 *        Verifies if the argument passed in the first position of
 *        the argv array it's an integer.
 *        "argc" will decrease by 1 if an integer is found.
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that containded the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's on
 *             position 0.
 *
 * @param data Reference to a string that will contain the data
 *             contained in argv[0] if it was valid. (DYNAMICALLY
 *             RESERVED INSIDE, NEED TO FREE FROM OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_integer(int *argc, char **argv, char **data);

/**
 * @brief Checks if an argument has a "HH:MM:SS" syntax.
 *        Verifies if the argument passed in the first position of
 *        the argv array it's an interval.
 *        "argc" will decrease by 1 if an interval is found.
 *        Intervals format is "HH:MM:SS" and can be negative.
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that containded the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's on
 *             position 0.
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_interval(int *argc, char **argv);

/**
 * @brief Checks if an argument has a "string" syntax.
 *        Verifies if the argument passed in the first position of
 *        the argv array it's a string.
 *        "argc" will decrease by 1 if a string is found.
 *
 * @param argc Reference to the current count of elements in the
 *             arguments array. Will decrease the number of
 *             positions in argv that containded the data to validate.
 *
 * @param argv Reference to the array of strings were the arguments
 *             are. Assumes that the argument to validate it's on
 *             position 0.
 *
 * @param data Reference to a string that will contain the data
 *             contained in argv[0] if it was valid. (DYNAMICALLY
 *             RESERVED INSIDE, NEED TO FREE FROM OUTSIDE).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_validate_string(int *argc, char **argv, char **data);

/**
 * @brief Converts a double to an interval.
 *        Example
 *            quantity = -61.5
 *            units = 'M'
 *            Result(interval) = "-01:01:30"
 *
 * @param quantity Amount of time to be converted
 *
 * @param units Units of the given time.
 *                  'S' = seconds.
 *                  'M' = minutes.
 *                  'H' = hours.
 *
 * @param interval Reference to a string in which the result
 *                 will be returned. (DYNAMICALLY RESERVED INSIDE,
 *                 NEED TO FREE FROM OUTSIDE).
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_double_to_interval(double quantity, const char units,
                             char **interval);

/**
 * @brief Escapes a string making it a regex.
 *        Converts a string to a regular expresion so, it can be used
 *        as one. It escapes all the special regular expression
 *        characters so they can be searched as simple characters.
 *        Result is returned into the same string (str).
 *
 *        Example
 *        Having the following string
 *            "[A string] ^with$ *special+ (characters)?."
 *        Result will be the following
 *            "\[A string\] \^with\$ \*special\+ \(characters\)\?\."
 *
 * @param str Reference to the string to be escaped.
 *
 * @param list_length Actual length of the string.
 *                    Equivalent to strlen(*str).
 *
 * @return int Return code that indicates if the operation was
 *             successful (ORCM_SUCCESS) or not.
 */
int query_str_to_regex(char **str, int act_length);

/* Opens a connection to the database. This function first checks if the
 * print component was not selected, otherwise an error will be returned.
 * If print component was selected means that the data for database connection
 * was not found.
 */
int open_database(void);

/* Perfoms a close operation in database framework */
int close_database(void);

/* Executes a query in the database */
int fetch_data_from_db(query_func_names db_func, opal_list_t *filters);

int query(query_func_names db_func, opal_list_t *filters, int query_limit);

/* Executes a get_next_row operation */
int query_get_next_row(opal_list_t *row);

/* Get the amount of rows returned from a executed query */
int get_query_num_rows(void);

/* Print all the results from a query. If there are rows to retrieve this
 * function will a get row and print it one by one. */
int print_all_query_results(time_t *query_time, int query_limit);

/* Print a row with the specified separator */
int print_row(opal_list_t *row, const char *separator, bool is_header);

/* Checks if the DB framework component is other than print component */
bool is_valid_db_component_selected(void);



END_C_DECLS

#endif /* ORCM_OCTL_QUERY_H */
