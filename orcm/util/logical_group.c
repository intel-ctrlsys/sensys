/*
 * Copyright (c) 2015      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "logical_group.h"
#include "orcm/constants.h"
#include "orte/util/regex.h"
#include "opal/mca/installdirs/installdirs.h"

#define VERBO 0
#define OUTID 0

#define MSG_HEADER ""
#define MSG_FOOTER "\n"

#define ORCM_OCTL_LGROUPING_EMSG0(verbosity, id, txt)       fprintf(stderr,MSG_HEADER"ERROR: "txt MSG_FOOTER)
#define ORCM_OCTL_LGROUPING_EMSG1(verbosity, id, txt, arg1) fprintf(stderr,MSG_HEADER"ERROR: "txt MSG_FOOTER,arg1)

#define SAFEFREE(p) if(NULL!=p) {free(p); p=NULL;}

void logro_pair_ctor(orcm_logro_pair_t *ptr)
{
    ptr->tag = NULL;
    ptr->nodename = NULL;
}
void logro_pair_dtor(orcm_logro_pair_t *ptr)
{
    SAFEFREE(ptr->tag);
    SAFEFREE(ptr->nodename);
}
OBJ_CLASS_INSTANCE(orcm_logro_pair_t, opal_list_item_t,
                   logro_pair_ctor, logro_pair_dtor);

opal_list_t LOGRO; //Global as requested.
orcm_lgroup_t LGROUP; //Global as requested.

int orcm_logical_group_init(void)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri && NULL == LGROUP.logro) {
        SAFEFREE(LGROUP.storage_filename);
        LGROUP.storage_filename = strdup("./orcm-logical_grouping.txt");
        if (NULL == LGROUP.storage_filename) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        erri = orcm_adjust_logical_grouping_path(opal_install_dirs.prefix);
        if( ORCM_SUCCESS != erri) {
            break;
        }

        OBJ_CONSTRUCT(&LOGRO, opal_list_t);

        LGROUP.logro = &LOGRO;

        break;
    }
    return erri;
}

int orcm_logical_group_delete(void)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        if (NULL !=LGROUP.logro) {
            OBJ_DESTRUCT(LGROUP.logro);
            LGROUP.logro = NULL; //Not owned.
        }

        SAFEFREE(LGROUP.storage_filename);

        break;
    }
    return erri;
}

int is_comment(const char * in_line)
{
    if (NULL == in_line ||
        '\0' == in_line[0] ||
        '\n' == in_line[0] ||
        '\r' == in_line[0] ||
        '#'  == in_line[0]
       )
    {
        return 1;
    }
    return 0;
}

int is_tag(const char * in_line)
{
    if (is_comment(in_line)) {
        return 0;
    }
    if (' '  == in_line[0] ||
        '\t' == in_line[0]
       )
    {
        return 0;
    }
    return 1;
}

int is_attribute(const char * in_line)
{
    if (is_comment(in_line)) {
        return 0;
    }
    if (is_tag(in_line)) {
        return 0;
    }
    return 1;
}

void trim(char * in_line, char ** o_new_start)
{
    if (NULL == in_line || NULL == o_new_start) {
        return;
    }
    char * p = in_line;
    while('\0' != *p){
        if (' ' == *p || '\t' == *p) {
            ++p;
            continue;
        }
        break;
    }
    *o_new_start = p;

    while('\0' != *p) {
        if (' ' == *p || '\t' == *p) {
            *p = '\0';
            break;
        }
        ++p;
        continue;
    }
}

int get_newline(FILE * in_fin, char * io_line, int in_max_line_length,
                      int * o_eof_found)
{
    *o_eof_found = 0;

    if (NULL == in_fin || NULL == io_line) {
        return ORCM_ERR_BAD_PARAM;
    }

    char * ret = fgets(io_line, in_max_line_length-1, in_fin); //-1 to give space for ending '\0'
    if (NULL == ret) {
        *o_eof_found = 1;
        return ORCM_SUCCESS;
    }

    if ('\0' != io_line[0]) {
        io_line[strlen(io_line)-1] = '\0';  /* remove newline */
    }

    return ORCM_SUCCESS;
}

int orcm_grouping_op_add(int argc, char **argv, opal_list_t * io_group)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            ORCM_OCTL_LGROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. Two needed, %d provided.", argc -2);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group) {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"During addition,bad grouping provided.");
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

int orcm_grouping_op_remove(int argc, char **argv, opal_list_t * io_group)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (4 != argc) {
            ORCM_OCTL_LGROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. Two needed, %d provided.", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group || opal_list_is_empty(io_group)) {
            //There is no logical grouping. Nothing to remove.
            break;
        }

        char * tag = argv[2];
        char * noderegex = argv[3];

        orcm_logro_pair_t * itr = NULL;
        orcm_logro_pair_t * next = NULL;

        int do_all_tag = is_do_all_wildcard(tag);

        if (is_do_all_wildcard(noderegex)) {
            OPAL_LIST_FOREACH_SAFE(itr, next, io_group, orcm_logro_pair_t) {
                if (do_all_tag || 0 == strcmp(tag, itr->tag)) {
                    opal_list_remove_item(io_group, &itr->super);
                    OBJ_RELEASE(itr);
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
                OPAL_LIST_FOREACH_SAFE(itr, next, io_group, orcm_logro_pair_t) {
                    if (0 == strcmp(nodname, itr->nodename)) {
                        if (do_all_tag || 0 == strcmp(tag, itr->tag)) {
                            opal_list_remove_item(io_group, &itr->super);
                            OBJ_RELEASE(itr);
                        }
                    }
                }
            }
        }

        break;
    }

    if (NULL == nodelist) {
        opal_argv_free(nodelist);
        nodelist=NULL;
        sz_nodelist = 0;
    }

    return erri;
}

int grouping_parse_from_file(opal_list_t * io_group, const char * in_filename,
                             int * o_file_missing)
{
    FILE * fin = NULL;
    char * linebuf = NULL;
    const int max_sz_linebuf = 4096;

#   define alternate_argc 4
    char * alternate_argv[alternate_argc] = {0};
    int i;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        *o_file_missing = 0;

        if (NULL == io_group || NULL == in_filename || '\0' == in_filename[0]) {
            erri = ORTE_ERR_BAD_PARAM;
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Bad setup for parsing logical groupings.");
            break;
        }

        fin = fopen(in_filename, "r");
        if (NULL == fin) {
            if ( ENOENT == errno) {
                *o_file_missing = 1;
                break;
            }
            erri = ORTE_ERR_FILE_OPEN_FAILURE;
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Failed to open file for logical groupings.");
            break;
        }

        linebuf = (char*) malloc( max_sz_linebuf * sizeof(char));
        if (NULL == linebuf) {
            erri = ORTE_ERR_OUT_OF_RESOURCE;
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Failed to allocate line buffer for logical groupings.");
            break;
        }

        for(i = 2; i < alternate_argc; ++i) {
            alternate_argv[i] = (char*) calloc(max_sz_linebuf, sizeof(char));
            if (NULL == alternate_argv[i]) {
                erri = ORTE_ERR_OUT_OF_RESOURCE;
                break;
            }
        }

        while (1) {
            int eof_found = 0;
            linebuf[0] = '\0';
            erri = get_newline(fin, linebuf, max_sz_linebuf, &eof_found);
            if (ORCM_SUCCESS != erri) {
                break;
            }

            if (1 == eof_found) {
                break;
            }

            if (is_comment(linebuf)) {
                continue;
            } else if (is_tag(linebuf)) {
                char *b;
                trim(linebuf, &b);
                sprintf(alternate_argv[2],"%s", b);
            } else if (is_attribute(linebuf)) {
                if ('\0' == alternate_argv[2][0]) {
                    //We are missing a tag.
                    ORCM_OCTL_LGROUPING_EMSG1(VERBO,OUTID,"Missing a tag for attribute : %s.", linebuf);
                    erri = ORTE_ERR_BAD_PARAM;
                    break;
                }
                char * p;
                trim(linebuf,&p);
                if (0 == strlen(p)) {
                    continue;
                }
                sprintf(alternate_argv[3],"%s", p);

                erri = orcm_grouping_op_add(alternate_argc, alternate_argv, io_group);
                if (ORTE_SUCCESS != erri) {
                    break;
                }
            } else {
                continue;
            }
        }
        if (ORCM_SUCCESS != erri) {
            break;
        }

        break;
    }

    for(i = 0; i < alternate_argc; ++i) {
        SAFEFREE(alternate_argv[i]);
    }

    SAFEFREE(linebuf);

    if (NULL != fin) {
        fclose(fin);
        fin = NULL;
    }

    return erri;
}

int grouping_save_to_file(opal_list_t * io_group, const char * in_filename)
{
    FILE * fout = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {

        fout = fopen(in_filename, "w");
        if (NULL == fout) {
            erri = ORTE_ERR_FILE_OPEN_FAILURE;
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Failed to open file for logical groupings.");
            break;
        }

        if (opal_list_is_empty(io_group)) {
            fprintf(fout, "# Empty logical grouping set\n");
            break;
        }

        orcm_logro_pair_t * itr = NULL;
        OPAL_LIST_FOREACH(itr, io_group, orcm_logro_pair_t) {
            if (NULL == itr->tag || NULL == itr->nodename) {
                continue;
            }
            if ('\0' == itr->tag[0] || '\0' == itr->nodename[0]) {
                continue;
            }
            fprintf(fout, "%s\n %s\n", itr->tag, itr->nodename);
        }

        break;
    }

    if (NULL != fout) {
        fclose(fout);
        fout = NULL;
    }

    return erri;
}

int orcm_grouping_op_load(int argc, char **argv, opal_list_t * io_group)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            ORCM_OCTL_LGROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. One needed, %d provided.", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        const char * filename = argv[2];

        int file_missing = 0;
        erri = grouping_parse_from_file(io_group, filename, &file_missing);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if(file_missing) {
            break;
        }
        break;
    }
    return erri;
}

int orcm_grouping_op_save(int argc, char **argv, opal_list_t * io_group)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if (3 != argc) {
            ORCM_OCTL_LGROUPING_EMSG1(VERBO,OUTID,"Incorrect argument count. One needed, %d provided.", argc);
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if (NULL == io_group) {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Missing logical group.");
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

int is_do_all_wildcard(const char * in_text)
{
    int answer = 0;
    if (NULL == in_text) {
        return answer;
    }
    unsigned long sz_txt = strlen(in_text);
    if(1 == sz_txt && '*' == in_text[0]) {
        answer = 1;
    }
    return answer;
}

int orcm_grouping_list(char * in_tag, char * in_node_regex, unsigned int * o_count,
                       char ** *o_tags, char ** *o_nodes)
{
    char ** nodelist = NULL;
    int sz_nodelist = 0;

    unsigned int count = 0;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (NULL == in_tag || NULL == in_node_regex || NULL == o_count ||
            NULL == o_tags || NULL == o_nodes)
        {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Invalid logical grouping internal state: grouping list");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        *o_count = 0;
        *o_tags = NULL;
        *o_nodes = NULL;

        if( ! opal_list_is_empty(LGROUP.logro)){
            OBJ_DESTRUCT(LGROUP.logro);
            OBJ_CONSTRUCT(LGROUP.logro, opal_list_t);
        }

        char * local_argv[4] = {0};

        local_argv[2] = LGROUP.storage_filename;
        erri = orcm_grouping_op_load(3, &local_argv[0], LGROUP.logro);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        int do_all_tag = is_do_all_wildcard(in_tag);

        //First count the entries than allocate and fill.
        count = 0;

        orcm_logro_pair_t * itr = NULL;
        if (is_do_all_wildcard(in_node_regex)) {
            OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                if (do_all_tag || 0 == strcmp(in_tag, itr->tag)) {
                    ++count;
                }
            }
        } else {
            erri = orte_regex_extract_node_names(in_node_regex, &nodelist);
            if (ORTE_SUCCESS != erri) {
                break;
            }
            sz_nodelist = opal_argv_count(nodelist);

            int k;
            for (k = 0; k < sz_nodelist; ++k) {
                const char * nodname = nodelist[k];
                OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                    if (0 == strcmp(nodname, itr->nodename) &&
                        (do_all_tag || 0 == strcmp(in_tag, itr->tag)) )
                    {
                        ++count;
                    }
                }
            }
        }

        ++count; //Add one for the ending NULL pointer.

        *o_tags = (char **) calloc(count, sizeof(char*));
        if (NULL == *o_tags) {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for tag array in logical grouping.");
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }
        *o_nodes = (char **) calloc(count, sizeof(char*));
        if (NULL == *o_nodes) {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for node array in logical grouping.");
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        itr = NULL;
        *o_count = 0;
        if (is_do_all_wildcard(in_node_regex)) {
            OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                if (do_all_tag || 0 == strcmp(in_tag, itr->tag)) {
                    char * tp = NULL;
                    int checkt = asprintf(&tp, "%s", itr->tag);
                    if (-1 == checkt) {
                        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a tag in logical grouping.");
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    char * np = NULL;
                    int checkn = asprintf(&np, "%s", itr->nodename);
                    if (-1 == checkn) {
                        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a node in logical grouping.");
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    (*o_tags)[*o_count] = tp; tp = NULL;
                    (*o_nodes)[*o_count] = np; np = NULL;
                    ++(*o_count);
                }
            }
        } else {
            erri = orte_regex_extract_node_names(in_node_regex, &nodelist);
            if (ORTE_SUCCESS != erri) {
                break;
            }
            sz_nodelist = opal_argv_count(nodelist);

            int k;
            for (k = 0; k < sz_nodelist; ++k) {
                const char * nodname = nodelist[k];
                OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logro_pair_t) {
                    if (0 == strcmp(nodname, itr->nodename) &&
                        (do_all_tag || 0 == strcmp(in_tag, itr->tag)) )
                    {
                        char * tp = NULL;
                        int checkt = asprintf(&tp, "%s", itr->tag);
                        if (-1 == checkt) {
                            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a tag in logical grouping.");
                            erri = ORCM_ERR_OUT_OF_RESOURCE;
                            break;
                        }
                        char * np = NULL;
                        int checkn = asprintf(&np, "%s", itr->nodename);
                        if (-1 == checkn) {
                            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a node in logical grouping.");
                            erri = ORCM_ERR_OUT_OF_RESOURCE;
                            break;
                        }
                        (*o_tags)[*o_count] = tp; tp = NULL;
                        (*o_nodes)[*o_count] = np; np = NULL;
                        ++(*o_count);
                    }
                }
            }
        }

        break;
    }

    if (ORCM_SUCCESS != erri) {
        if (NULL != o_tags) {
            opal_argv_free(*o_tags);
            *o_tags = NULL;
        }
        if (NULL != o_nodes) {
            opal_argv_free(*o_nodes);
            *o_nodes = NULL;
        }
        if (NULL != o_count) {
            *o_count = 0;
        }
    }

    opal_argv_free(nodelist);
    nodelist = NULL;
    sz_nodelist = 0;

    return erri;
}

int orcm_grouping_listnodes(char * in_tag,
                            unsigned int * o_count, char ** o_csvlist_nodes)
{
    char * csv = NULL;
    char ** tags  = NULL;
    char ** nodes = NULL;

    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (NULL == in_tag || '\0' == in_tag[0] ||
            NULL == o_count || NULL == o_csvlist_nodes )
        {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        *o_count = 0;
        *o_csvlist_nodes = NULL;

        char noderegex[] = "*";

        erri = orcm_grouping_list(in_tag, noderegex, o_count, &tags, &nodes);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if(0 == *o_count){
            break;
        }

        unsigned int i=0;

        unsigned int length = 0;

        for (i=0; i < *o_count; ++i) {
            length += strlen(nodes[i]) + 1; //+1 for the separating comma
        }

        csv = (char*) calloc(length, sizeof(char));
        if (NULL == csv) {
            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for csv list.");
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        char * p = csv;
        for (i=0; i < *o_count; ++i) {
            sprintf(p, "%s", nodes[i]);
            p = csv + strlen(csv);
            if (i+1 != *o_count) {
                sprintf(p, ",");
            }
            ++p;
        }

        break;
    }

    opal_argv_free(tags);
    tags = NULL;
    opal_argv_free(nodes);
    nodes = NULL;

    if (ORCM_SUCCESS == erri) {
        *o_csvlist_nodes = csv;
        csv = NULL;
    } else {
        SAFEFREE(csv);
        if (NULL != o_count) {
            *o_count = 0;
        }
    }

    return erri;
}

int orcm_node_names(char *in_regexp, char ***o_names)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        erri = orcm_logical_group_init();
        if (ORCM_SUCCESS != erri) {
            break;
        }

        char * tag = in_regexp;
        if (NULL  == tag) {
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        while('\0' != *tag && (' ' == *tag || '\t' == *tag)) {
            ++tag;
        }

        if ('\0' == *tag) {
            fprintf(stderr, "\n  ERROR: Given node regex is empty.\n");
            erri = ORCM_ERR_BAD_PARAM;
            break;
        }

        if ('$' == tag[0]) {
            ++tag; //Omit the '$' character

            if ('\0' == tag[0]) {
                fprintf(stderr, "\n  ERROR: Given logical grouping tag is empty.\n");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            char * comma = strchr(tag, ',');
            char * openb = strchr(tag, '[');
            char * closeb= strchr(tag, ']');

            if (NULL != comma) {
                fprintf(stderr, "\n  ERROR: Given logical grouping tag must not be a csv list.\n");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            } else if (openb < closeb) {
                //If openb > closeb, than it is not a regex.
                fprintf(stderr, "\n  ERROR: Given logical grouping tag must not be a regex.\n");
                erri = ORCM_ERR_BAD_PARAM;
                break;
            }

            char * all_nodes = "*";
            unsigned int count = 0;
            char ** tags = NULL;
            char ** nodes = NULL;
            erri = orcm_grouping_list(tag, all_nodes,
                                      &count, &tags, &nodes);
            opal_argv_free(tags);
            tags = NULL;
            if (0 == count) {
                //This imitates what is done inside orte_regex_extract_node_names().
                *o_names = NULL;
                opal_argv_free(nodes);
            } else {
                *o_names = nodes;
                nodes = NULL;
            }
        } else {
            erri = orte_regex_extract_node_names(in_regexp, o_names);
        }
        break;
    }
    return erri;
}

int orcm_adjust_logical_grouping_path(char * in_install_dirs_prefix)
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        if(NULL == in_install_dirs_prefix) {
            //Not much can be done without a user directory.
            //So use default.
            break;
        }

        char * newf = NULL;
        int check = asprintf(&newf, "%s/etc/orcm-logical_grouping.txt",
                             in_install_dirs_prefix);
        if (-1 == check) {
            erri = ORCM_ERR_OUT_OF_RESOURCE;
            break;
        }

        if (NULL == newf) {
            //Nothing much more can be done.  So let everything as is.
            break;
        } else {
            SAFEFREE(LGROUP.storage_filename);
            LGROUP.storage_filename = newf;
            newf = NULL;
        }
        break;
    }
    return erri;
}
