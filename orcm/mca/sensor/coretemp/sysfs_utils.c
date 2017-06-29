/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#define _GNU_SOURCE

#define TEMPERATURE_FILE_REGEX "coretemp.[[:digit:]]+/([[:alnum:]]+/)*temp[[:digit:]]+_(input|label)"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <regex.h>

#include "sysfs_utils.h"

static char temperature_files_path[512];

void clean_temperature_files_path(void)
{
    memset(temperature_files_path, '\0', sizeof(temperature_files_path));
}

int find_temp_files(const char *filepath, const struct stat *info,
                    const int typeflag, struct FTW *pathinfo)
{
    regex_t regex;
    if (regcomp(&regex, TEMPERATURE_FILE_REGEX, REG_EXTENDED))
        return 0;

    if (regexec(&regex, filepath, 0, NULL, 0))
        return 0;

    clean_temperature_files_path();
    memcpy(temperature_files_path, filepath, pathinfo->base);
    return 1;
}

char* get_temperature_files_path(const char* current_dir)
{
    if (NULL == current_dir)
        return NULL;

    nftw(current_dir, find_temp_files, 1, 0);
    if ('\0' == temperature_files_path[0])
        return NULL;
    return strdup(temperature_files_path);
}
