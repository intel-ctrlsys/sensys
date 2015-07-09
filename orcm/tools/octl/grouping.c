/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orcm/tools/octl/common.h"
#include "orte/util/regex.h"

#define VERBO 0
#define OUTID 0

#define MSG0(verbosity, id, txt) fprintf(stderr,txt)
#define MSG1(verbosity, id, txt, arg1) fprintf(stderr,txt,arg1)

static void logro_pair_ctor(orcm_logro_pair_t *ptr)
{
    ptr->tag = NULL;
    ptr->nodename = NULL;
}
static void logro_pair_dtor(orcm_logro_pair_t *ptr)
{
    if (NULL != ptr->tag) {
        free(ptr->tag);
        ptr->tag = NULL;
    }
    if (NULL != ptr->nodename) {
        free(ptr->nodename);
        ptr->nodename = NULL;
    }
}
OBJ_CLASS_INSTANCE(orcm_logro_pair_t, opal_list_item_t,
                   logro_pair_ctor, logro_pair_dtor);

static int grouping_parse_from_file(opal_list_t * io_group,
                                    const char * in_filename)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (NULL == io_group || NULL == in_filename || '\0' == in_filename[0]) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        printf("TODO: parse in_filename =  %s\n", in_filename);

        break;
    }
    return erri;
}

static int grouping_save_to_file(opal_list_t * io_group,
                                 const char * in_filename)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        printf("TODO: emit in_filename =  %s\n", in_filename);

        break;
    }
    return erri;
}

int orcm_octl_grouping_load(int argc, char **argv, opal_list_t * io_group)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. One needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        const char * filename = argv[2];

        erri = grouping_parse_from_file(io_group, filename);
        if (ORCM_SUCCESS != erri) {
            break;
        }
        break;
    }
    return erri;
}

int orcm_octl_grouping_add(int argc, char **argv, opal_list_t * io_group)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. Two needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group) {
            MSG0(VERBO,OUTID,"\nDuring addition,bad grouping provided.\n");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        const char * tag = argv[2];
        char * noderegex = argv[3];

        erri = orte_regex_extract_node_names(noderegex, &nodelist);
        if (ORTE_SUCCESS != erri) {
            break;
        }
        sz_nodelist = opal_argv_count(nodelist);

        int i;
        for (i=0; i<sz_nodelist; ++i) {
            orcm_logro_pair_t * p = NULL;
            p = OBJ_NEW(orcm_logro_pair_t);
            if (NULL == p) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            p->tag = strdup(tag);
            p->nodename = strdup(nodelist[i]);
            opal_list_append(io_group, &p->super);
            p = NULL;
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }
        break;
    }

    opal_argv_free(nodelist);
    nodelist = NULL;
    sz_nodelist = 0;

    return erri;
}

int orcm_octl_grouping_remove(int argc, char **argv, opal_list_t * io_group)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. Two needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group || opal_list_is_empty(io_group)) {
            //There is no logical grouping. Nothing to remove.
            break;
        }

        const char * tag = argv[2];
        char * noderegex = argv[3];

        erri = orte_regex_extract_node_names(noderegex, &nodelist);
        if (ORTE_SUCCESS != erri) {
            break;
        }
        sz_nodelist = opal_argv_count(nodelist);

        int k;
        for (k = 0; k < sz_nodelist; ++k) {
            const char * nodname = nodelist[k];
            orcm_logro_pair_t * itr = NULL;
            orcm_logro_pair_t * next = NULL;
            OPAL_LIST_FOREACH_SAFE(itr, next, io_group, orcm_logro_pair_t) {
                if (0 == strcmp(nodname, itr->nodename)) {
                    if (0 == strcmp(tag, itr->tag)) {
                        opal_list_remove_item(io_group, &itr->super);
                        OBJ_RELEASE(itr);
                    }
                }
            }
        }

        break;
    }

    opal_argv_free(nodelist);
    nodelist=NULL;
    sz_nodelist = 0;

    return erri;
}

int orcm_octl_grouping_save(int argc, char **argv, opal_list_t * io_group)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. One needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group || opal_list_is_empty(io_group)) {
            //There is no logical grouping.
            break;
        }

        const char * filename = argv[2];

        erri = grouping_save_to_file(io_group, filename);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }
    return erri;
}

int orcm_octl_grouping_listnode(int argc, char **argv, opal_list_t * io_group)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. One needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group || opal_list_is_empty(io_group)) {
            //There is no logical grouping. Nothing to list out.
            break;
        }

        const char * tag = argv[2];

        orcm_logro_pair_t * itr = NULL;
        OPAL_LIST_FOREACH(itr, io_group, orcm_logro_pair_t) {
            if (0 == strcmp(tag, itr->tag)) {
                fprintf(stdout,"%s\n", itr->nodename);
            }
        }
        break;
    }
    return erri;
}

int orcm_octl_grouping_listtag(int argc, char **argv, opal_list_t * io_group)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            MSG1(VERBO,OUTID,"\nIncorrect argument count. One needed, %d provided.\n", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group || opal_list_is_empty(io_group)) {
            //There is no logical grouping. Nothing to list out.
            break;
        }

        char * noderegex = argv[2];

        erri = orte_regex_extract_node_names(noderegex, &nodelist);
        if (ORTE_SUCCESS != erri) {
            break;
        }
        sz_nodelist = opal_argv_count(nodelist);

        int k;
        for (k = 0; k < sz_nodelist; ++k) {
            const char * nodname = nodelist[k];
            orcm_logro_pair_t * itr = NULL;
            OPAL_LIST_FOREACH(itr, io_group, orcm_logro_pair_t) {
                if (0 == strcmp(nodname, itr->nodename)) {
                    fprintf(stdout,"%s\n", itr->tag);
                }
            }
        }

        break;
    }

    opal_argv_free(nodelist);
    nodelist = NULL;
    sz_nodelist = 0;

    return erri;
}


