/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte/util/regex.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/regex.h"

int orcm_regex_string_cmp(const void *inputa, const void *inputb);

int orcm_regex_string_cmp(const void *inputa, const void *inputb)
{
    const char **stringa = (const char **)inputa;
    const char **stringb = (const char **)inputb;
    return strcmp(*stringa, *stringb);
}

int orcm_regex_sort(char **nodes) {
    size_t nodeslen = 0;
    size_t shift;

    while ('\0' != nodes[nodeslen]) {
        nodeslen++;
    }

    /* sort array using qsort */
    qsort(nodes, nodeslen, sizeof(char *), orcm_regex_string_cmp);
    
    nodeslen = 1;
    shift = 1;
    
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

int orcm_regex_create(char *nodes, char **regexp) {
    char **node_array;
    
    node_array = opal_argv_split_with_empty(nodes, ',');
    orcm_regex_sort(node_array);
    nodes = opal_argv_join(node_array, ',');
    printf("sorted: %s\n", nodes);
    orte_regex_create(nodes, regexp);
    
    opal_argv_free(node_array);

    return ORCM_SUCCESS;
}

int orcm_regex_extract_node_names(char *regexp, char ***names) {
    return orte_regex_extract_node_names(regexp, names);
    
    return ORCM_SUCCESS;
}

int orcm_regex_extract_names_list(char *regexp, char **namelist){
    char **names = NULL;
    
    orte_regex_extract_node_names(regexp, &names);
    *namelist = opal_argv_join(names, ',');

    free(names);
    
    return ORCM_SUCCESS;
}

int orcm_regex_add_node(char **regexp, char *name) {
    char **nodes;
    char *nodelist;
    size_t nodeslen = 0;
    
    orte_regex_extract_node_names(*regexp, &nodes);
 
    while (NULL != nodes[nodeslen]) {
        nodeslen++;
    }

    nodes = realloc(nodes, (nodeslen + 2) * sizeof(char *));
    nodes[nodeslen] = name;
    nodes[nodeslen+1] = '\0';
    orcm_regex_sort(nodes);
    
    nodelist = opal_argv_join(nodes, ',');
    orcm_regex_create(nodelist, regexp);
    
    free(nodes);
    free(nodelist);
    
    return ORCM_SUCCESS;
}

int orcm_regex_remove_node(char **regexp, char *name) {
    char **nodes;
    char **new_nodes;
    char *nodelist;
    size_t nodeslen = 0;
    size_t i = 0;
    bool found = false;

    orte_regex_extract_node_names(*regexp, &nodes);
    
    while (NULL != nodes[nodeslen]) {
        nodeslen++;
    }

    new_nodes = malloc(nodeslen * sizeof(char *));

    for (i = 0; i < nodeslen; i++) {
        if (0  == strcmp(nodes[i], name)) {
            found = true;
            continue;
        }
        if (found) {
            new_nodes[i-1] = nodes[i];
        } else {
            new_nodes[i] = nodes[i];
        }
    }
    
    new_nodes[i-1] = '\0';
    
    if (found) {
        nodelist = opal_argv_join(new_nodes, ',');
        orcm_regex_create(nodelist, regexp);
        free(nodelist);
    }

    free(nodes);
    free(new_nodes);
    
    return ORCM_SUCCESS;
}
