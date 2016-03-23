/*
 * Copyright (c) 2016     Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <regex.h>

#include "orcm/util/utils.h"
#include "orcm/tools/octl/common.h"

char *search_msg(regex_t regex_label);
int regex_label( regex_t *regex, char *type, char *label);

const char *next_label = "^[[:space:]]*\\[[^:]+[^]]+\\][[:space:]]*$";

/**
 * @brief Function that takes a string label and finds the message
 *        related to the label in the "messages.txt" file.
 *
 * @param regex_label Pre-compiled message label pattern.
 *
 * @return char* Message or NULL if the message wasn't found.
 *               DON'T FORGET TO FREE!
 */
char *search_msg(regex_t regex_label) {
    FILE *file = NULL;
    char line[1024] = {0,};
    int regex_res = 0;
    char *msg = NULL;
    char *datadir = NULL;
    regex_t regex;

    asprintf(&datadir,"%s%s", OCTL_DATA_DIR,"/messages.txt");

    regex_res = regcomp(&regex, next_label, REG_EXTENDED);

    file = fopen(datadir, "r");

    if (file && !regex_res) {
        while (NULL != fgets(line, 1024, file)) {
            regex_res = regexec(&regex_label, line, 0, NULL, 0);
            if (!regex_res){
                msg = strdup("\0");
                while ((fgets(line, 1024, file)) != NULL) {
                    if (!regexec(&regex, line, 0, NULL, 0)) {
                         break;
                    }
                    msg = (char *) realloc(msg, strlen(msg) + strlen(line) + 1);
                    strcat(msg, line);
                }
                break;
            }
        }
        regfree(&regex);
        fclose(file);
    }

    SAFEFREE(datadir);
    return msg;
}

/**
 * @brief Function that creates and compiles a regular expresion into a form
 *        that is used to find a message label.
 *
 * @param regex Pointer to a label pattern buffer.
 *
 * @param type  Type of message.
 *
 * @param label Defined message label.
 *
 * @return int Returns 0 for a succeful compilation or an error code for a
 *             failure.
 */


int regex_label( regex_t *regex, char *type, char *label) {
      char *new_label = NULL;
      char *regex_exp = "^\\[%s\\:%s\\]";
      int rc = 0;

      asprintf(&new_label, regex_exp, type, label);
      rc = regcomp(regex, new_label, REG_EXTENDED);

      SAFEFREE(new_label);
      return rc;
}


/**
 * @brief Function to show revelant information regarding the operation
 *        executed on octl, this will print the message to standard output.
 *
 * @param label Defined message label.
 */
void orcm_octl_info(char *label, ...){
    char *info_msg = NULL;
    char *output = NULL;
    regex_t r_label;

    va_list var;
    va_start (var,label);

    if (!regex_label(&r_label, "info", label)) {
        info_msg = search_msg(r_label);
        if (NULL != info_msg) {
            vasprintf(&output, info_msg, var);
            fprintf(stdout, output);
        }
        regfree(&r_label);
    }

    va_end(var);
    SAFEFREE(output);
    SAFEFREE(info_msg);
}

/**
 * @brief For error messages on octl operations, this will print a
 *        pre-defined message to stderr.
 *
 * @param label Defined message label.
 */
void orcm_octl_error(char *label, ...){
    char *error_msg = NULL;
    char *output = NULL;
    regex_t r_label;

    va_list var;
    va_start(var, label);

    if (!regex_label( &r_label,"error", label)) {
        error_msg = search_msg(r_label);

        if (NULL != error_msg) {
            vasprintf(&output, error_msg, var);
            fprintf(stderr,"ERROR: %s", output);
            SAFEFREE(output);
            SAFEFREE(error_msg);
        } else {
            fprintf(stderr,"ERROR: Unkown error.");
        }
        regfree(&r_label);
    } else {
      fprintf(stderr,"ERROR: Unable to generate message.");
    }

   va_end(var);
}
