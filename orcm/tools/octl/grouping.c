/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orcm/tools/octl/common.h"
#include "orcm/util/logical_group.h"
#include "orte/util/regex.h"

#define VERBO 0
#define OUTID 0

#define MSG_HEADER ""
#define MSG_FOOTER "\n"

#define ORCM_OCTL_GROUPING_MSG0(verbosity, id, txt)       fprintf(stdout,MSG_HEADER txt MSG_FOOTER)
#define ORCM_OCTL_GROUPING_MSG2(verbosity, id, txt, arg1, arg2) fprintf(stdout,MSG_HEADER txt MSG_FOOTER,arg1,arg2)

#define ORCM_OCTL_GROUPING_EMSG0(verbosity, id, txt)       fprintf(stderr,MSG_HEADER"ERROR: "txt MSG_FOOTER)
#define ORCM_OCTL_GROUPING_EMSG1(verbosity, id, txt, arg1) fprintf(stderr,MSG_HEADER"ERROR: "txt MSG_FOOTER,arg1)

int orcm_octl_grouping_add(int argc, char **argv)
{
    char * local_argv[4] = {0};

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            ORCM_OCTL_GROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. Two needed, %d provided.", argc -2);
            ORCM_OCTL_GROUPING_EMSG0(VERBO,OUTID,"Correct syntax: [octl] grouping add <tag> <node>");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        char * tag = argv[2];
        char * noderegex = argv[3];

        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if( ! opal_list_is_empty(LGROUP.logro)){
            OBJ_DESTRUCT(LGROUP.logro);
            OBJ_CONSTRUCT(LGROUP.logro, opal_list_t);
        }

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_load(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        local_argv[2] = tag;
        local_argv[3] = noderegex;
        erri = orcm_grouping_op_add(4, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_save(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        ORCM_OCTL_GROUPING_MSG0(VERBO,OUTID, "Data added.");

        break;
    }

    return erri;
}

int orcm_octl_grouping_remove(int argc, char **argv)
{
    char * local_argv[4] = {0};

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            ORCM_OCTL_GROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. Two needed, %d provided.", argc -2);
            ORCM_OCTL_GROUPING_EMSG0(VERBO,OUTID,"Correct syntax: [octl] grouping remove <tag> <node>");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if( ! opal_list_is_empty(LGROUP.logro)){
            OBJ_DESTRUCT(LGROUP.logro);
            OBJ_CONSTRUCT(LGROUP.logro, opal_list_t);
        }

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_load(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        char * tag = argv[2];
        char * noderegex = argv[3];

        local_argv[2] = tag;
        local_argv[3] = noderegex;

        erri = orcm_grouping_op_remove(4, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_save(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        ORCM_OCTL_GROUPING_MSG0(VERBO,OUTID, "Data removed.");

        break;
    }
    return erri;
}

int orcm_octl_grouping_list(int argc, char **argv)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    char * local_argv[4] = {0};

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            ORCM_OCTL_GROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. Two needed, %d provided.", argc -2);
            ORCM_OCTL_GROUPING_EMSG0(VERBO,OUTID,"Correct syntax: [octl] grouping list <tag> <node>");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if( ! opal_list_is_empty(LGROUP.logro)){
            OBJ_DESTRUCT(LGROUP.logro);
            OBJ_CONSTRUCT(LGROUP.logro, opal_list_t);
        }

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_load(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (opal_list_is_empty(LGROUP.logro))
        {
            //There is no logical grouping. Nothing to list out.
            ORCM_OCTL_GROUPING_MSG0(VERBO,OUTID, "\n");
            break;
        }

        char * tag = argv[2];
        char * noderegex = argv[3];

        int do_all_tag = is_do_all_wildcard(tag);

        orcm_logro_pair_t * itr = NULL;
        if (is_do_all_wildcard(noderegex)) {
            OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                if (do_all_tag || 0 == strcmp(tag, itr->tag)) {
                    ORCM_OCTL_GROUPING_MSG2(VERBO,OUTID, "%s\n %s", itr->tag, itr->nodename);
                }
            }
        } else {
            erri = orte_regex_extract_node_names(noderegex, &nodelist);
            if (ORTE_SUCCESS != erri) {
                break;
            }
            sz_nodelist = opal_argv_count(nodelist);

            int k;
            for (k = 0; k < sz_nodelist; ++k) {
                const char * nodname = nodelist[k];
                OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                    if (0 == strcmp(nodname, itr->nodename) &&
                        (do_all_tag || 0 == strcmp(tag, itr->tag)) )
                    {
                        ORCM_OCTL_GROUPING_MSG2(VERBO,OUTID, "%s\n %s", itr->tag, itr->nodename);
                    }
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


