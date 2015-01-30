/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * Copyright (c) 2014      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>

#include "opal/class/opal_list.h"
#include "opal/util/argv.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_path.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"

#include "orte/util/show_help.h"
#include "orte/util/error_strings.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rmaps/rmaps_types.h"

#include "orcm/mca/pwrmgmt/pwrmgmt.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "orcm/mca/pwrmgmt/base/pwrmgmt_freq_utils.h"

static bool orcm_pwrmgmt_freq_initialized = false;

typedef struct {
    opal_list_item_t super;
    int core;
    char *directory;
    /* save the system settings so we can restore them when we die */
    char *system_governor;
    float system_max_freq;
    float system_min_freq;
    /* save the current settings so we only change them when required */
    char *current_governor;
    float current_max_freq;
    float current_min_freq;
    /* keep a list of allowed values */
    opal_list_t governors;
    opal_list_t frequencies;
    /* mark if setspeed is supported */
    bool setspeed;
} pwrmgmt_freq_tracker_t;

static void ctr_con(pwrmgmt_freq_tracker_t *trk)
{
    trk->directory = NULL;
    trk->system_governor = NULL;
    trk->current_governor = NULL;
    OBJ_CONSTRUCT(&trk->governors, opal_list_t);
    OBJ_CONSTRUCT(&trk->frequencies, opal_list_t);
    trk->setspeed = false;
}

static void ctr_des(pwrmgmt_freq_tracker_t *trk)
{
    if (NULL != trk->directory) {
        free(trk->directory);
    }
    if (NULL != trk->system_governor) {
        free(trk->system_governor);
    }
    if (NULL != trk->current_governor) {
        free(trk->current_governor);
    }
    OPAL_LIST_DESTRUCT(&trk->governors);
    OPAL_LIST_DESTRUCT(&trk->frequencies);
}
OBJ_CLASS_INSTANCE(pwrmgmt_freq_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

static char *orte_getline(FILE *fp)
{
    char *ret, *buff;
    char input[1024];
    int k;

    ret = fgets(input, 1024, fp);
    if (NULL != ret) {
        /* trim the end of the line */
        for (k=strlen(input)-1; 0 < k && isspace(input[k]); k--) {
            input[k] = '\0';
        }
        buff = strdup(input);
        return buff;
    }
    
    return NULL;
}

static opal_list_t tracking;

int orcm_pwrmgmt_freq_init(void)
{
    int k;
    DIR *cur_dirp = NULL;
    struct dirent *entry;
    char *filename, *tmp, **vals;
    FILE *fp;
    pwrmgmt_freq_tracker_t *trk;
    opal_value_t *kv;

    /* are we already initialized */
    if(orcm_pwrmgmt_freq_initialized) {
        return ORCM_SUCCESS;
    }

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/sys/devices/system/cpu"))) {
        OBJ_DESTRUCT(&tracking);
        if (4 < opal_output_get_verbosity(orcm_pwrmgmt_base_framework.framework_output)) {
            orte_show_help("help-rtc-freq.txt", "req-dir-not-found",
                           true, orte_process_info.nodename,
                           "/sys/devices/system/cpu");
        }
        return ORTE_ERROR;
    }

    /*
     * For each directory
     */
    while (NULL != (entry = readdir(cur_dirp))) {
        
        /*
         * Skip the obvious
         */
        if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
            0 == strncmp(entry->d_name, "..", strlen(".."))) {
            continue;
        }

        /* look for cpu directories */
        if (0 != strncmp(entry->d_name, "cpu", strlen("cpu"))) {
            /* cannot be a cpu directory */
            continue;
        }
        /* if it ends in other than a digit, then it isn't a cpu directory */
        if (!isdigit(entry->d_name[strlen(entry->d_name)-1])) {
            continue;
        }

        /* track the info for this core */
        trk = OBJ_NEW(pwrmgmt_freq_tracker_t);
        /* trailing digits are the core id */
        for (k=strlen(entry->d_name)-1; 0 <= k; k--) {
            if (!isdigit(entry->d_name[k])) {
                k++;
                break;
            }
        }
        trk->core = strtoul(&entry->d_name[k], NULL, 10);
        trk->directory = opal_os_path(false, "/sys/devices/system/cpu", entry->d_name, "cpufreq", NULL);
        
        /* read/save the current settings */
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        if (NULL == (fp = fopen(filename, "rw"))) {
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }
        trk->system_governor = orte_getline(fp);
        trk->current_governor = strdup(trk->system_governor);
        fclose(fp);
        free(filename);

        filename = opal_os_path(false, trk->directory, "scaling_max_freq", NULL);
        if (NULL == (fp = fopen(filename, "rw"))) {
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }
        tmp = orte_getline(fp);
        fclose(fp);
        trk->system_max_freq = strtoul(tmp, NULL, 10) / 1000000.0;
        trk->current_max_freq = trk->system_max_freq;
        free(filename);
        free(tmp);

        filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
        if (NULL == (fp = fopen(filename, "rw"))) {
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }
        tmp = orte_getline(fp);
        fclose(fp);
        trk->system_min_freq = strtoul(tmp, NULL, 10) / 1000000.0;
        trk->current_min_freq = trk->system_min_freq;
        free(filename);
        free(tmp);

        /* get the list of available governors */
        filename = opal_os_path(false, trk->directory, "scaling_available_governors", NULL);
        if (NULL == (fp = fopen(filename, "r"))) {
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }
        tmp = orte_getline(fp);
        fclose(fp);
        free(filename);
        if (NULL != tmp) {
            vals = opal_argv_split(tmp, ' ');
            free(tmp);
            for (k=0; NULL != vals[k]; k++) {
                kv = OBJ_NEW(opal_value_t);
                kv->type = OPAL_STRING;
                kv->data.string = strdup(vals[k]);
                opal_list_append(&trk->governors, &kv->super);
            }
            opal_argv_free(vals);
        }

        /* get the list of available frequencies */
        filename = opal_os_path(false, trk->directory, "scaling_available_frequencies", NULL);
        if (NULL == (fp = fopen(filename, "r"))) {
            free(filename);
            OBJ_RELEASE(trk);
            continue;
        }
        tmp = orte_getline(fp);
        fclose(fp);
        free(filename);
        if (NULL != tmp) {
            vals = opal_argv_split(tmp, ' ');
            free(tmp);
            for (k=0; NULL != vals[k]; k++) {
                kv = OBJ_NEW(opal_value_t);
                kv->type = OPAL_FLOAT;
                kv->data.fval = strtoul(vals[k], NULL, 10) / 1000000.0;
                opal_list_append(&trk->frequencies, &kv->super);
            }
            opal_argv_free(vals);
        }

        /* see if setspeed is supported */
        filename = opal_os_path(false, trk->directory, "scaling_setspeed", NULL);
        if (NULL != (fp = fopen(filename, "rw"))) {
            trk->setspeed = true;
            fclose(fp);
        }
        free(filename);

        /* add to our list */
        opal_list_append(&tracking, &trk->super);
    }
    closedir(cur_dirp);

    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to read */
        if (0 < opal_output_get_verbosity(orcm_pwrmgmt_base_framework.framework_output)) {
            orte_show_help("help-rtc-freq.txt", "no-cores-found",
                           true, orte_process_info.nodename);
        }
        OPAL_LIST_DESTRUCT(&tracking);
        return ORTE_ERROR;
    }

    /* report out the results, if requested */
    if (9 < opal_output_get_verbosity(orcm_pwrmgmt_base_framework.framework_output)) {
        OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
            opal_output(0, "%s\tCore: %d  Governor: %s MaxFreq: %f MinFreq: %f\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), trk->core,
                        trk->system_governor, trk->system_max_freq, trk->system_min_freq);
            OPAL_LIST_FOREACH(kv, &trk->governors, opal_value_t) {
                opal_output(0, "%s\t\tGovernor: %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), kv->data.string);
            }
            OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
                opal_output(0, "%s\t\tFrequency: %f",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), kv->data.fval);
            }
        }
    }

    orcm_pwrmgmt_freq_initialized = true;

    return ORTE_SUCCESS;
}

void orcm_pwrmgmt_freq_finalize(void)
{
    OPAL_LIST_DESTRUCT(&tracking);

    orcm_pwrmgmt_freq_initialized = false;

    return;
}

int orcm_pwrmgm_freq_get_num_cpus(void)
{   
    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return 0;
        }
    }

    return opal_list_get_size(&tracking);
}

int orcm_pwrmgmt_freq_set_governor(int cpu, char* governor)
{
    char *tmp, **vals;
    pwrmgmt_freq_tracker_t *trk;
    opal_value_t *kv;
    bool allowed;
    char *filename;
    FILE *fp;
    int count = 0;

    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if (cpu != trk->core && -1 != cpu) {
            continue;
        }
    
        if (-1 != cpu) {
            count = cpu;
        }

        /* does the requested value match the current setting? */
        if (0 == strcmp(trk->current_governor, governor)) {
            break;
        }
        /* is the specified governor among those allowed? */
        allowed = false;
        OPAL_LIST_FOREACH(kv, &trk->governors, opal_value_t) {
            if (0 == strcmp(kv->data.string, governor)) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            vals = NULL;
            OPAL_LIST_FOREACH(kv, &trk->governors, opal_value_t) {
                opal_argv_append_nosize(&vals, kv->data.string);
            }
            tmp = opal_argv_join(vals, ',');
            opal_argv_free(vals);
            free(tmp);
            
            return ORCM_ERR_NOT_SUPPORTED;
        }
        /* attempt to set the value */
        filename = opal_os_path(false, trk->directory, "scaling_governor", NULL);
        if (NULL == (fp = fopen(filename, "w"))) {
            free(filename);
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
        opal_output_verbose(2, orcm_pwrmgmt_base_framework.framework_output,
                            "%s Setting governor %s for cpu %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), governor, count);
        fprintf(fp, "%s\n", governor);
        fclose(fp);
        free(filename);
        free(trk->current_governor);
        trk->current_governor = strdup(governor);
        if(-1 != cpu) {
            break;
        }
        count++;
    }
    return ORCM_SUCCESS;
}

int orcm_pwrmgmt_freq_set_min_freq(int cpu, float freq)
{
    char *tmp, **vals;
    pwrmgmt_freq_tracker_t *trk;
    opal_value_t *kv;
    bool allowed;
    char *filename;
    FILE *fp;
    int count = 0;

    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if (cpu != trk->core && -1 != cpu) {
            continue;
        }

        if (-1 != cpu) {
            count = cpu;
        }

        /* does the requested value match the current setting? */
        if (trk->current_min_freq == freq) {
            break;
        }
        /* is the specified frequency among those allowed? */
        allowed = false;
        OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
            if (kv->data.fval == freq) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            vals = NULL;
            OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
                asprintf(&tmp, "%f", kv->data.fval);
                opal_argv_append_nosize(&vals, tmp);
                free(tmp);
            }
            tmp = opal_argv_join(vals, ',');
            opal_argv_free(vals);
            free(tmp);
                
            return ORCM_ERR_NOT_SUPPORTED;
        }
        filename = opal_os_path(false, trk->directory, "scaling_min_freq", NULL);
        /* attempt to set the value */
        if (NULL == (fp = fopen(filename, "w"))) {
            /* not allowed - report the error */
            orte_show_help("help-rtc-freq.txt", "permission-denied", true,
                           "min freq", orte_process_info.nodename, filename);
            free(filename);
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
        opal_output_verbose(2, orcm_pwrmgmt_base_framework.framework_output,
                            "%s Setting min freq controls to %ld for cpu %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            (unsigned long)(freq * 1000000.0),
                            count);
        fprintf(fp, "%ld\n", (unsigned long)(freq * 1000000.0));
        fclose(fp);
        free(filename);
        trk->current_min_freq = freq;
        if (-1 != cpu) {
            break;
        }
        count++;
    }
    return ORCM_SUCCESS;
}


int orcm_pwrmgmt_freq_set_max_freq(int cpu, float freq)
{
    char *tmp, **vals;
    pwrmgmt_freq_tracker_t *trk;
    opal_value_t *kv;
    bool allowed;
    char *filename;
    FILE *fp;
    int count = 0;

    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if (cpu != trk->core && -1 != cpu) {
            continue;
        }

        if (-1 != cpu) {
            count = cpu;
        }

        /* does the requested value match the current setting? */
        if (trk->current_max_freq == freq) {
            break;
        }
        /* is the specified frequency among those allowed? */
        allowed = false;
        OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
            if (kv->data.fval == freq) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            vals = NULL;
            OPAL_LIST_FOREACH(kv, &trk->frequencies, opal_value_t) {
                asprintf(&tmp, "%f", kv->data.fval);
                opal_argv_append_nosize(&vals, tmp);
                free(tmp);
            }
            tmp = opal_argv_join(vals, ',');
            opal_argv_free(vals);
            free(tmp);
                
            return ORCM_ERR_NOT_SUPPORTED;
        }
        filename = opal_os_path(false, trk->directory, "scaling_max_freq", NULL);
        /* attempt to set the value */
        if (NULL == (fp = fopen(filename, "w"))) {
            /* not allowed - report the error */
            orte_show_help("help-rtc-freq.txt", "permission-denied", true,
                           "min freq", orte_process_info.nodename, filename);
            free(filename);
            return ORCM_ERR_FILE_WRITE_FAILURE;
        }
        opal_output_verbose(2, orcm_pwrmgmt_base_framework.framework_output,
                            "%s Setting max freq controls to %ld for cpu %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            (unsigned long)(freq * 1000000.0),
                            count);
        fprintf(fp, "%ld\n", (unsigned long)(freq * 1000000.0));
        fclose(fp);
        free(filename);
        trk->current_max_freq = freq;
        if(-1 != cpu) {
            break;
        }
        count++;
    }
    return ORCM_SUCCESS;
}

int orcm_pwrmgmt_freq_get_supported_governors(int cpu, opal_list_t** governors)
{
    pwrmgmt_freq_tracker_t *trk;

    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if (cpu != trk->core) {
            continue;
        }
       
        *governors = &trk->governors; 
        return ORCM_SUCCESS;
    }
    
    return ORCM_ERR_NOT_FOUND;
}

int orcm_pwrmgmt_freq_get_supported_frequencies(int cpu, opal_list_t** frequencies)
{
    pwrmgmt_freq_tracker_t *trk;
    
    if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if (cpu == trk->core) {
            continue;
        }
       
        *frequencies = &trk->frequencies; 
        return ORCM_SUCCESS;
    }
    
    return ORCM_ERR_NOT_FOUND;
}

int orcm_pwrmgmt_freq_reset_system_settings(void)
{
    pwrmgmt_freq_tracker_t *trk;
    int rc = ORCM_SUCCESS;
    int err = ORCM_SUCCESS;

   if(!orcm_pwrmgmt_freq_initialized) {
        if(ORCM_SUCCESS != orcm_pwrmgmt_freq_init()) {
            return ORCM_ERR_NOT_INITIALIZED;
        }
    }

    /* loop thru all the cpus on this node */
    OPAL_LIST_FOREACH(trk, &tracking, pwrmgmt_freq_tracker_t) {
        if(ORCM_SUCCESS != (err = orcm_pwrmgmt_freq_set_governor(trk->core, trk->system_governor))) {
            rc = err;
        }
        if(ORCM_SUCCESS != (err = orcm_pwrmgmt_freq_set_min_freq(trk->core, trk->system_min_freq))) {
            rc = err;
        }
        if(ORCM_SUCCESS != (err = orcm_pwrmgmt_freq_set_max_freq(trk->core, trk->system_max_freq))) {
            rc = err;
        }
    }
 
    return rc;
}


