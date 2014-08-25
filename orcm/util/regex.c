/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/regex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/regex.h"

int orcm_regex_string_cmp(const void *inputa, const void *inputb);

/* string compare function for sort */
int orcm_regex_string_cmp(const void *inputa, const void *inputb)
{
    const char **stringa = (const char **)inputa;
    const char **stringb = (const char **)inputb;
    return strcmp(*stringa, *stringb);
}

/* sort the array of strings and trim out duplicates */
int orcm_regex_sort(char **nodes) {
    size_t nodeslen = 0;
    size_t shift;
    
    /* NULL array is sorted */
    if (NULL == nodes) {
        return ORCM_SUCCESS;
    }

    /* look for NULL elements, and count elements */
    do {
        if (NULL == nodes[nodeslen]) {
            ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
            return ORCM_ERR_BAD_PARAM;
        }
        nodeslen++;
    } while ('\0' != nodes[nodeslen]);

    /* sort array using qsort, 
     * but mergesort might be better for adding and removing nodes */
    qsort(nodes, nodeslen, sizeof(char *), orcm_regex_string_cmp);
    
    nodeslen = 1;
    shift = 1;

    /* this will trim duplicates by shifting non-duplicates down
     * and then adding the termination string at the end
     * of the non-duplist
     * there will still be elements past the termination string
     * but they should be ignored by our functions
     */
    while ('\0' != nodes[nodeslen]) {
        if ( 0 != strcmp(nodes[nodeslen], nodes[nodeslen-1])) {
            nodes[shift] = nodes[nodeslen];
            shift++;
        }
        nodeslen++;
    }
    nodes[shift] = '\0';
    
    return ORCM_SUCCESS;
}

/* sort array, then pass to orte's regex function */
int orcm_regex_create(char *nodes, char **regexp) {
    char **node_array = NULL;
    int rc;
    
    /* split with empty will terminate our list appropriately */
    node_array = opal_argv_split_with_empty(nodes, ',');
    if (ORCM_SUCCESS != (rc = orcm_regex_sort(node_array))) {
        ORTE_ERROR_LOG(rc);
        opal_argv_free(node_array);
        return rc;
    }
    nodes = opal_argv_join(node_array, ',');
    if (ORTE_SUCCESS != (rc = orte_regex_create(nodes, regexp))) {
        ORTE_ERROR_LOG(rc);
        opal_argv_free(node_array);
        return rc;
    }
    
    opal_argv_free(node_array);
    return ORCM_SUCCESS;
}

/* take regex, set node array, just pass this to orte regex */
int orcm_regex_extract_node_names(char *regexp, char ***names) {
    int rc;
    
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(regexp, names))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    return ORCM_SUCCESS;
}

/* take regex, set a string of comma separated node names */
int orcm_regex_extract_names_list(char *regexp, char **namelist){
    char **names = NULL;
    int rc;
    
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(regexp, &names))) {
        ORTE_ERROR_LOG(rc);
        if (names) {
            free(names);
        }
        return rc;
    }
    
    *namelist = opal_argv_join(names, ',');

    free(names);
    return ORCM_SUCCESS;
}

/* add a name to a regex */
int orcm_regex_add_node(char **regexp, char *name) {
    char **nodes = NULL;
    char *nodelist = NULL;
    size_t nodeslen = 0;
    int rc;
    
    if (NULL == name) {
        return ORCM_SUCCESS;
    } else if (NULL == regexp) {
        return orcm_regex_create(name, regexp);
    }
    
    /* extract regex to an array */
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(*regexp, &nodes))) {
        ORTE_ERROR_LOG(rc);
        if (nodes) {
            free(nodes);
        }
        return rc;
    }
 
    /* get size of array */
    if (NULL == nodes) {
        nodeslen = 0;
    } else {
        while (NULL != nodes[nodeslen]) {
            nodeslen++;
        }
    }

    /* we need two more array items, our new name and a terminating string */
    nodes = realloc(nodes, (nodeslen + 2) * sizeof(char *));
    nodes[nodeslen] = name;
    nodes[nodeslen+1] = '\0';
    /* sort the new array */
    if (ORCM_SUCCESS != (rc = orcm_regex_sort(nodes))) {
        ORTE_ERROR_LOG(rc);
        free(nodes);
        return rc;
    }
    
    nodelist = opal_argv_join(nodes, ',');
    /* create the resulting regex */
    if (ORCM_SUCCESS != (rc = orcm_regex_create(nodelist, regexp))) {
        ORTE_ERROR_LOG(rc);
        free(nodes);
        free(nodelist);
        return rc;
    }
    
    free(nodes);
    free(nodelist);
    return ORCM_SUCCESS;
}

/* remove name from regex */
int orcm_regex_remove_node(char **regexp, char *name) {
    char **nodes = NULL;
    char *nodelist = NULL;
    size_t nodeslen = 0;
    size_t i = 0;
    bool found = false;
    int rc;

    if (NULL == regexp || NULL == name) {
        return ORCM_SUCCESS;
    }
    if (ORTE_SUCCESS != (rc = orte_regex_extract_node_names(*regexp, &nodes))) {
        ORTE_ERROR_LOG(rc);
        if (nodes) {
            free(nodes);
        }
        return rc;
    }
    
    /* get size of array */
    if (NULL == nodes) {
        nodeslen = 0;
    } else {
        while (NULL != nodes[nodeslen]) {
            nodeslen++;
        }
    }

    /* shift names down on top of element we want to remove */
    for (i = 0; i < nodeslen; i++) {
        if (0  == strcmp(nodes[i], name)) {
            found = true;
            continue;
        }
        if (found) {
            nodes[i-1] = nodes[i];
        }
    }
    
    nodes[i-1] = '\0';
    
    /* if we found the element to remove, create a new regex,
     * otherwise just leave the regex alone */
    if (found) {
        nodelist = opal_argv_join(nodes, ',');
        if (ORCM_SUCCESS != (rc = orcm_regex_create(nodelist, regexp))) {
            ORTE_ERROR_LOG(rc);
        }
        free(nodelist);
        free(nodes);
        return rc;
    }

    free(nodes);
    return ORCM_SUCCESS;
}
