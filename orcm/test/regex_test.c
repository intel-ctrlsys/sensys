#include "orcm_config.h"

#include <stdio.h>
#include <unistd.h>

#include "orte/util/proc_info.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/util/regex.h"
#include "orcm/runtime/runtime.h"

int main(int argc, char **argv)
{
    char *nodes;
    char *node;
    char *regexp;
    
    fprintf(stdout, "test simple node order\n");
    nodes = strdup("node1,node2,node3");
    fprintf(stdout, "nodes: %s\n", nodes);
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(regexp);
 
    fprintf(stdout, "test padding variances\n");
    nodes = strdup("node1,node002,node3");
    fprintf(stdout, "nodes: %s\n", nodes);
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node002,node1,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(regexp);

    fprintf(stdout, "test repeated node\n");
    nodes = strdup("node1,node1,node3");
    fprintf(stdout, "nodes: %s\n", nodes);
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(regexp);

    fprintf(stdout, "test out of order\n");
    nodes = strdup("node1,node3,node2");
    fprintf(stdout, "nodes: %s\n", nodes);
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(regexp);
    
    fprintf(stdout, "test multi-prefix out of order\n");
    nodes = strdup("node1,test4,node2,test3");
    fprintf(stdout, "nodes: %s\n", nodes);
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,test3,test4")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(regexp);
    
    fprintf(stdout, "test node addition\n");
    nodes = strdup("node1,node2,node4");
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    fprintf(stdout, "adding node3\n");
    node = strdup("node3");
    orcm_regex_add_node(&regexp, node);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,node3,node4")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(node);
    free(nodes);
    free(regexp);
    
    fprintf(stdout, "test repeated node addition\n");
    nodes = strdup("node1,node2,node4");
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    fprintf(stdout, "adding node2\n");
    node = strdup("node2");
    orcm_regex_add_node(&regexp, node);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,node4")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(node);
    free(nodes);
    free(regexp);
    
    fprintf(stdout, "test node removal\n");
    nodes = strdup("node1,node2,node3");
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    fprintf(stdout, "removing node2\n");
    node = strdup("node2");
    orcm_regex_remove_node(&regexp, node);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(node);
    free(nodes);
    free(regexp);

    fprintf(stdout, "test bad node removal\n");
    nodes = strdup("node1,node2,node3");
    orcm_regex_create(nodes, &regexp);
    free(nodes);
    fprintf(stdout, "regex: %s\n", regexp);
    fprintf(stdout, "removing node4\n");
    node = strdup("node4");
    orcm_regex_remove_node(&regexp, node);
    fprintf(stdout, "regex: %s\n", regexp);
    orcm_regex_extract_names_list(regexp, &nodes);
    if (0 == strcmp(nodes, "node1,node2,node3")) {
        fprintf(stdout, "PASSED: ");
    } else {
        fprintf(stdout, "FAIL: ");
    }
    fprintf(stdout, "nodes: %s\n\n", nodes);
    free(node);
    free(nodes);
    free(regexp);

    return ORCM_SUCCESS;
}
