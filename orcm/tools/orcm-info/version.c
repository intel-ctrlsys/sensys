/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2010-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"

#include <stdio.h>
#include <string.h>

#include "opal/version.h"
#include "orcm/version.h"
#include "opal/mca/base/base.h"
#include "opal/util/printf.h"

#include "orcm/runtime/orcm_info_support.h"
#include "orcm/tools/orcm-info/orcm-info.h"

/*
 * Public variables
 */

const char *orcm_info_ver_full = "full";
const char *orcm_info_ver_major = "major";
const char *orcm_info_ver_minor = "minor";
const char *orcm_info_ver_release = "release";
const char *orcm_info_ver_greek = "greek";
const char *orcm_info_ver_repo = "repo";

/*
 * Private variables
 */

static const char *orcm_info_ver_all = "all";
static const char *orcm_info_ver_mca = "mca";
static const char *orcm_info_ver_type = "type";
static const char *orcm_info_ver_component = "component";


/*
 * Private functions
 */

static void show_mca_version(const mca_base_component_t *component,
                             const char *scope, const char *ver_type);
static char *make_version_str(const char *scope,
                              int major, int minor, int release,
                              const char *greek, 
                              bool want_repo, const char *repo);

/*
 * do_version
 *
 * Determines the version information related to the orcm components
 * being used.
 * Accepts: 
 *	- want_all: True if all components' info is required.
 *	- cmd_line: The constructed command line argument
 */
void orcm_info_do_version(bool want_all, opal_cmd_line_t *cmd_line)
{
    unsigned int count;
    size_t i;
    char *arg1, *scope, *type, *component;
    char *pos;
    int j;
    
    orcm_info_components_open();
    
    if (want_all) {
        orcm_info_show_orcm_version(orcm_info_ver_full);
        for (j = 0; j < mca_types.size; ++j) {
            if (NULL == (pos = (char*)opal_pointer_array_get_item(&mca_types, j))) {
                continue;
            }
            orcm_info_show_component_version(pos, orcm_info_component_all, orcm_info_ver_full, orcm_info_type_all);
        }
    } else {
        count = opal_cmd_line_get_ninsts(cmd_line, "version");
        for (i = 0; i < count; ++i) {
            arg1 = opal_cmd_line_get_param(cmd_line, "version", (int)i, 0);
            scope = opal_cmd_line_get_param(cmd_line, "version", (int)i, 1);
            
            /* Version of Open MPI */
            
            if (NULL != arg1 && NULL != scope && 0 == strcmp(orcm_info_type_orcm, arg1)) {
                orcm_info_show_orcm_version(scope);
            } 
            
            /* Specific type and component */
            
            else if (NULL != (pos = strchr(arg1, ':'))) {
                *pos = '\0';
                type = arg1;
                pos++;
                component = pos;
                
                orcm_info_show_component_version(type, component, scope, orcm_info_ver_all);
                
            }
            
            /* All components of a specific type */
            
            else {
                orcm_info_show_component_version(arg1, orcm_info_component_all, scope, orcm_info_ver_all);
            }
        }
    }
}


/*
 * Show all the components of a specific type/component combo (component may be
 * a wildcard)
 */
void orcm_info_show_component_version(const char *type_name, 
                                      const char *component_name,
                                      const char *scope, const char *ver_type)
{
    bool want_all_components = false;
    bool found;
    opal_list_item_t *item;
    mca_base_component_list_item_t *cli;
    const mca_base_component_t *component;
    opal_list_t *components;
    int j;
    char *pos;
    orcm_info_component_map_t *map;
    
    /* see if all components wanted */
    if (0 == strcmp(orcm_info_type_all, component_name)) {
        want_all_components = true;
    }
    
    /* Check to see if the type is valid */
    for (found = false, j = 0; j < mca_types.size; ++j) {
        if (NULL == (pos = (char*)opal_pointer_array_get_item(&mca_types, j))) {
            continue;
        }
        if (0 == strcmp(pos, type_name)) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        exit(1);
    }
    
    /* Now that we have a valid type, find the right component list */
    components = NULL;
    for (j=0; j < component_map.size; j++) {
        if (NULL == (map = (orcm_info_component_map_t*)opal_pointer_array_get_item(&component_map, j))) {
            continue;
        }
        if (0 == strcmp(type_name, map->type)) {
            /* found it! */
            components = map->components;
            break;
        }
    }

    if (NULL != components) {
        if (opal_list_get_size(components) > 0){
            for (item = opal_list_get_first(components);
                 opal_list_get_end(components) != item;
                 item = opal_list_get_next(item)) {
                cli = (mca_base_component_list_item_t *) item;
                component = cli->cli_component;
                if (want_all_components || 
                    0 == strcmp(component->mca_component_name, component_name)) {
                    show_mca_version(component, scope, ver_type);
                }
            }
        }
    }
}


/*
 * Given a component, display its relevant version(s)
 */
static void show_mca_version(const mca_base_component_t* component,
                             const char *scope, const char *ver_type)
{
    bool printed;
    bool want_mca = false;
    bool want_type = false;
    bool want_component = false;
    char *message, *content;
    char *mca_version;
    char *api_version;
    char *component_version;
    char *tmp;
    
    if (0 == strcmp(ver_type, orcm_info_ver_all) ||
        0 == strcmp(ver_type, orcm_info_ver_mca)) {
        want_mca = true;
    }
    
    if (0 == strcmp(ver_type, orcm_info_ver_all) ||
        0 == strcmp(ver_type, orcm_info_ver_type)) {
        want_type = true;
    }
    
    if (0 == strcmp(ver_type, orcm_info_ver_all) ||
        0 == strcmp(ver_type, orcm_info_ver_component)) {
        want_component = true;
    }
    
    mca_version = make_version_str(scope, component->mca_major_version,
                                   component->mca_minor_version,
                                   component->mca_release_version, "",
                                   false, "");
    api_version = make_version_str(scope, component->mca_type_major_version,
                                   component->mca_type_minor_version,
                                   component->mca_type_release_version, "",
                                   false, "");
    component_version = make_version_str(scope, component->mca_component_major_version,
                                         component->mca_component_minor_version,
                                         component->mca_component_release_version, 
                                         "", false, "");
    
    if (orcm_info_pretty) {
        asprintf(&message, "MCA %s", component->mca_type_name);
        printed = false;
        asprintf(&content, "%s (", component->mca_component_name);
        
        if (want_mca) {
            asprintf(&tmp, "%sMCA v%s", content, mca_version);
            free(content);
            content = tmp;
            printed = true;
        }

        if (want_type) {
            if (printed) {
                asprintf(&tmp, "%s, ", content);
                free(content);
                content = tmp;
            }
            asprintf(&tmp, "%sAPI v%s", content, api_version);
            free(content);
            content = tmp;
            printed = true;
        }

        if (want_component) {
            if (printed) {
                asprintf(&tmp, "%s, ", content);
                free(content);
                content = tmp;
            }
            asprintf(&tmp, "%sComponent v%s", content, component_version);
            free(content);
            content = tmp;
            printed = true;
        }
        if (NULL != content) {
            asprintf(&tmp, "%s)", content);
            free(content);
        } else {
            tmp = NULL;
        }
        
        if (NULL != tmp) {
            orcm_info_out(message, NULL, tmp);
            free(message);
            free(tmp);
        }
        
    } else {
        asprintf(&message, "mca:%s:%s:version", component->mca_type_name, component->mca_component_name);
        if (want_mca) {
            asprintf(&tmp, "mca:%s", mca_version);
            orcm_info_out(NULL, message, tmp);
            free(tmp);
        }
        if (want_type) {
            asprintf(&tmp, "api:%s", api_version);
            orcm_info_out(NULL, message, tmp);
            free(tmp);
        }
        if (want_component) {
            asprintf(&tmp, "component:%s", component_version);
            orcm_info_out(NULL, message, tmp);
            free(tmp);
        }
        free(message);
    }

    free (mca_version);
    free (api_version);
    free (component_version);
}


static char *make_version_str(const char *scope,
                               int major, int minor, int release,
                               const char *greek, 
                               bool want_repo, const char *repo)
{
    char *str = NULL, *tmp;
    char temp[BUFSIZ];
    
    temp[BUFSIZ - 1] = '\0';
    if (0 == strcmp(scope, orcm_info_ver_full) ||
        0 == strcmp(scope, orcm_info_ver_all)) {
        snprintf(temp, BUFSIZ - 1, "%d.%d", major, minor);
        str = strdup(temp);
        if (release > 0) {
            snprintf(temp, BUFSIZ - 1, ".%d", release);
            asprintf(&tmp, "%s%s", str, temp);
            free(str);
            str = tmp;
        }
        if (NULL != greek) {
            asprintf(&tmp, "%s%s", str, greek);
            free(str);
            str = tmp;
        }
        if (want_repo && NULL != repo) {
            asprintf(&tmp, "%s%s", str, repo);
            free(str);
            str = tmp;
        }
    } else if (0 == strcmp(scope, orcm_info_ver_major)) {
        snprintf(temp, BUFSIZ - 1, "%d", major);
    } else if (0 == strcmp(scope, orcm_info_ver_minor)) {
        snprintf(temp, BUFSIZ - 1, "%d", minor);
    } else if (0 == strcmp(scope, orcm_info_ver_release)) {
        snprintf(temp, BUFSIZ - 1, "%d", release);
    } else if (0 == strcmp(scope, orcm_info_ver_greek)) {
        str = strdup(greek);
    } else if (0 == strcmp(scope, orcm_info_ver_repo)) {
        str = strdup(repo);
    }
    
    if (NULL == str) {
        str = strdup(temp);
    }
    
    return str;
}
