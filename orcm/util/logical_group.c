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

static int orcm_logical_group_is_valid_tag(char *tag);
static int orcm_logical_group_tag_to_nodes(char *tag, char ***o_names);
static char *orcm_logical_group_is_valid_noderegex(char *in_regexp);

void logical_group_pair_construct(orcm_logical_group_node_t *ptr)
{
    ptr->node = NULL;
}

void logical_group_pair_destruct(orcm_logical_group_node_t *ptr)
{
    SAFEFREE(ptr->node);
}

OBJ_CLASS_INSTANCE(orcm_logical_group_node_t, opal_list_item_t,
                   logical_group_pair_construct, logical_group_pair_destruct);

opal_list_t GROUPS; //Global as requested.
orcm_logical_group_t LGROUP = {NULL, NULL}; //Global as requested.

int orcm_logical_group_init(void)
{
    int erri = -1;

    if (NULL == LGROUP.groups) {
        SAFEFREE(LGROUP.storage_filename);
        LGROUP.storage_filename = strdup("./orcm-logical_grouping.txt");
        if (NULL == LGROUP.storage_filename) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }

        erri = orcm_adjust_logical_grouping_path(opal_install_dirs.prefix);
        if( ORCM_SUCCESS != erri) {
            return erri;
        }

        LGROUP.groups = OBJ_NEW(opal_hash_table_t);
        if (NULL == LGROUP.groups) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    return ORCM_SUCCESS;
}

int orcm_logical_group_delete(void)
{
    if (NULL != LGROUP.groups) {
        OBJ_RELEASE(LGROUP.groups);
    }

    SAFEFREE(LGROUP.storage_filename);

    return ORCM_SUCCESS;
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

int is_nodelist(const char * in_line)
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

    if (NULL == in_fin || NULL == io_line || 0 == in_max_line_length) {
        return ORCM_ERR_BAD_PARAM;
    }

    char * ret = fgets(io_line, in_max_line_length-1, in_fin); //-1 to give space for ending '\0'
    if (NULL == ret) {
        *o_eof_found = 1;
        return ORCM_SUCCESS;
    }

    unsigned long sz = strlen(io_line);
    if (0 != sz) {
        io_line[sz-1] = '\0';  /* remove newline */
    }

    return ORCM_SUCCESS;
}

static bool node_exist(opal_list_t *group_nodes, char *new_node)
{
    bool exist = false;
    orcm_logical_group_node_t *node_item = NULL;
    if (NULL == new_node) {
        return exist;
    }

    OPAL_LIST_FOREACH(node_item, group_nodes, orcm_logical_group_node_t) {
        if (NULL == node_item) {
            continue;
        }

        if (0 == strncmp(node_item->node, new_node, strlen(node_item->node))) {
            exist = true;
            break;
        }
    }

    return exist;
}

int orcm_grouping_op_add(char *tag, char *noderegex, opal_hash_table_t *io_group)
{
    char **nodelist = NULL;
    int index = 0, size = 0, erri = ORCM_SUCCESS;
    opal_list_t *group_nodes = NULL;
    orcm_logical_group_node_t *node_item = NULL;

    if (NULL == io_group) {
        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"During addition,bad grouping provided.");
        return ORCM_ERR_BAD_PARAM;
    }

    erri = orte_regex_extract_node_names(noderegex, &nodelist);
    if (ORTE_SUCCESS != erri) {
        opal_argv_free(nodelist);
        return ORCM_ERR_BAD_PARAM;
    }
    size = opal_argv_count(nodelist);

    erri = opal_hash_table_get_value_ptr(io_group, tag, strlen(tag), &group_nodes);
    if (OPAL_ERR_NOT_FOUND == erri || NULL == group_nodes) {
        group_nodes = OBJ_NEW(opal_list_t);
        if (NULL == group_nodes) {
            opal_argv_free(nodelist);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
    }

    for (index = 0; index < size; ++index) {
        if (false == node_exist(group_nodes, nodelist[i])) {
            node_item = OBJ_NEW(orcm_logical_group_node_t);
            if (NULL == node_item) {
                erri = ORCM_ERR_OUT_OF_RESOURCE;
                break;
            }
            node_item->node = strdup(nodelist[i]);
            opal_list_append(group_nodes, &node_item->super);
            node_item = NULL;
        }
    }

    if (erri == ORCM_SUCCESS) {
        erri = opal_hash_table_set_value_ptr(io_group, tag, strlen(tag), group_nodes);
    }

    opal_argv_free(nodelist);
    nodelist = NULL;

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

        orcm_logical_group_node_t * itr = NULL;
        orcm_logical_group_node_t * next = NULL;

        int do_all_tag = is_do_all_wildcard(tag);

        if (is_do_all_wildcard(noderegex)) {
            OPAL_LIST_FOREACH_SAFE(itr, next, io_group, orcm_logical_group_node_t) {
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
                OPAL_LIST_FOREACH_SAFE(itr, next, io_group, orcm_logical_group_node_t) {
                    if (0 == strcmp(nodname, itr->noderegx)) {
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

    if (NULL != nodelist) {
        opal_argv_free(nodelist);
        nodelist=NULL;
        sz_nodelist = 0;
    }

    return erri;
}

int grouping_parse_from_file(opal_list_t *io_group, const char *in_filename,
                             int *o_file_missing)
{
    FILE *fin = NULL;
    char *linebuf = NULL;
    char *key_tag = NULL;
    int eof = -1;

    int erri;
    *o_file_missing = 0;

    if (NULL == (fin = fopen(in_filename, "r"))) {
        if (ENOENT == errno) {
            *o_file_missing = 1;
            return ORCM_SUCCESS;
        }
        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Failed to open file for logical groupings.");
        return ORCM_ERR_FILE_OPEN_FAILURE;
    }

    linebuf = (char*)malloc(MAX_LINE_LENGTH * sizeof(char));
    if (NULL == linebuf) {
        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Failed to allocate line buffer for logical groupings.");
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    while (1) {
        eof = 0;
        linebuf[0] = '\0';
        erri = get_newline(fin, linebuf, MAX_LINE_LENGTH, &eof);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (1 == eof) {
            break;
        }

        if (is_comment(linebuf)) {
            continue;
        }

        if (is_tag(linebuf)) {
                char *b;
                trim(linebuf, &b);
                sprintf(alternate_argv[2],"%s", b);
            } else if (is_nodelist(linebuf)) {
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

        orcm_logical_group_node_t * itr = NULL;
        OPAL_LIST_FOREACH(itr, io_group, orcm_logical_group_node_t) {
            if (NULL == itr->tag || NULL == itr->noderegx) {
                continue;
            }
            if ('\0' == itr->tag[0] || '\0' == itr->noderegx[0]) {
                continue;
            }
            fprintf(fout, "%s\n %s\n", itr->tag, itr->noderegx);
        }

        break;
    }

    if (NULL != fout) {
        fclose(fout);
        fout = NULL;
    }

    return erri;
}

int orcm_grouping_op_load(char *storage_filename, opal_hash_table_t *io_group)
{
    int file_missing = 0;

    if (NULL == storage_filename || '\0' == storage_filename[0] || NULL == io_group) {
        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID,"Bad setup for parsing logical groupings.");
        return ORCM_ERR_BAD_PARAM;
    }

    return grouping_parse_from_file(io_group, storage_filename, &file_missing);

    while (ORCM_SUCCESS == erri) {
        file_missing = 0;
        if ()
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

        orcm_logical_group_node_t * itr = NULL;
        if (is_do_all_wildcard(in_node_regex)) {
            OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logical_group_node_t) {
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
                OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logical_group_node_t) {
                    if (0 == strcmp(nodname, itr->noderegx) &&
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
            OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logical_group_node_t) {
                if (do_all_tag || 0 == strcmp(in_tag, itr->tag)) {
                    char * tp = NULL;
                    int checkt = asprintf(&tp, "%s", itr->tag);
                    if (-1 == checkt) {
                        SAFEFREE(tp);
                        ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a tag in logical grouping.");
                        erri = ORCM_ERR_OUT_OF_RESOURCE;
                        break;
                    }
                    char * np = NULL;
                    int checkn = asprintf(&np, "%s", itr->noderegx);
                    if (-1 == checkn) {
                        SAFEFREE(tp);
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
                OPAL_LIST_FOREACH(itr, LGROUP.logro, orcm_logical_group_node_t) {
                    if (0 == strcmp(nodname, itr->noderegx) &&
                        (do_all_tag || 0 == strcmp(in_tag, itr->tag)) )
                    {
                        char * tp = NULL;
                        int checkt = asprintf(&tp, "%s", itr->tag);
                        if (-1 == checkt) {
                            SAFEFREE(tp);
                            ORCM_OCTL_LGROUPING_EMSG0(VERBO,OUTID, "Failed to allocate for a tag in logical grouping.");
                            erri = ORCM_ERR_OUT_OF_RESOURCE;
                            break;
                        }
                        char * np = NULL;
                        int checkn = asprintf(&np, "%s", itr->noderegx);
                        if (-1 == checkn) {
                            SAFEFREE(tp);
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

int orcm_logical_group_trim_noderegex(char *in_regexp, char **o_regexp)
{
    char *regexp = in_regexp;
    int erri = ORCM_SUCCESS;

    if (NULL != regexp) {
        while('\0' != *regexp && (' ' == *regexp || '\t' == *regexp)) {
            ++regexp;
        }

        if ('\0' == *regexp) {
            fprintf(stderr, "\n  ERROR: Given node regex is empty.\n");
            regexp = NULL;
            erri = ORCM_ERR_BAD_PARAM;
        }
    }

    *o_regexp = regexp;
    return erri;
}

int orcm_logical_group_is_valid_tag(char *tag)
{
    char *comma = NULL;
    char *openb = NULL;
    char *closeb = NULL;

    if ('\0' == tag[0]) {
        fprintf(stderr, "\n  ERROR: Given logical grouping tag is empty.\n");
        return ORCM_ERR_BAD_PARAM;
    }

    comma = strchr(tag, ',');
    openb = strchr(tag, '[');
    closeb= strchr(tag, ']');

    /* the regular expression can not include comma, nor the openb bigger than the closeb*/
    if (NULL != comma) {
        fprintf(stderr, "\n  ERROR: Given logical grouping tag must not be a csv list.\n");
        return ORCM_ERR_BAD_PARAM;
    }
    if (openb < closeb) {
        fprintf(stderr, "\n  ERROR: Given logical grouping tag must not be a regex.\n");
        return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

int orcm_logical_group_tag_to_nodes(char *tag, char ***o_names)
{
    char *all_nodes = "*";
    unsigned int count = 0;
    char **tags = NULL;
    char **nodes = NULL;
    int erri = orcm_logical_group_is_valid_tag(tag);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    erri = orcm_grouping_list(tag, all_nodes, &count, &tags, &nodes);
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

    return erri;
}

int orcm_logical_group_tag_to_nodelist(char *tag, char **o_nodelist)
{
    unsigned int count = 0;
    int erri = orcm_logical_group_is_valid_tag(tag);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    return orcm_grouping_listnodes(tag, &count, o_nodelist);
}

int orcm_logical_group_prepare_for_nodes(char *in_regexp, char **o_regexp)
{
    int erri = orcm_logical_group_trim_noderegex(in_regexp, o_regexp);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    return orcm_logical_group_init();
}

int orcm_node_names(char *in_regexp, char ***o_names)
{
    int erri = ORCM_SUCCESS;
    char *o_regexp = NULL;

    erri = orcm_logical_group_prepare_for_nodes(in_regexp, &o_regexp);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    /* If the node regex is a tag: starting with $ */
    if ('$' == o_regexp[0]) {
        ++o_regexp; //Omit the '$' character
        erri = orcm_logical_group_tag_to_nodes(o_regexp, o_names);
    } else {
        erri = orte_regex_extract_node_names(in_regexp, o_names);
    }

    return erri;
}

int orcm_node_names_list(char *in_regexp, char **o_nodelist)
{
    int erri = ORCM_SUCCESS;
    char *o_regexp = NULL;

    erri = orcm_logical_group_prepare_for_nodes(in_regexp, &o_regexp);
    if (ORCM_SUCCESS != erri) {
        return erri;
    }

    if ('$' == o_regexp[0]) {
        ++o_regexp;
        erri = orcm_logical_group_tag_to_nodelist(o_regexp, o_nodelist);
    } else {
        *o_nodelist = in_regexp;
    }

    return ORCM_SUCCESS;
}

int orcm_adjust_logical_grouping_path(char * in_install_dirs_prefix)
{
    int check = -1;
    char *new_file = NULL;

    if(NULL == in_install_dirs_prefix) {
        //Not much can be done without a user directory. So use default.
        return ORCM_SUCCESS;
    }

    check = asprintf(&new_file, "%s/etc/orcm-logical_grouping.txt", in_install_dirs_prefix);
    if (-1 == check) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if (NULL == new_file) {
        //Nothing much more can be done.  So let everything as is.
        return ORCM_SUCCESS;
    }

    SAFEFREE(LGROUP.storage_filename);
    LGROUP.storage_filename = new_file;
    new_file = NULL;

    return ORCM_SUCCESS;
}
