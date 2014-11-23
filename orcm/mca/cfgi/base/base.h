/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_CFGI_BASE_H
#define MCA_CFGI_BASE_H

/*
 * includes
 */
#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss_types.h"

#include "orte/types.h"

#include "orcm/mca/cfgi/cfgi.h"


/*
 * Global functions for MCA overall collective open and close
 */
BEGIN_C_DECLS

typedef struct {
    opal_list_t actives;
    char *config_file;
    int ping_rate;
    bool validate;
} orcm_cfgi_base_t;
ORCM_DECLSPEC extern orcm_cfgi_base_t orcm_cfgi_base;

typedef struct {
    opal_list_item_t super;
    int priority;
    orcm_cfgi_base_module_t *mod;
} orcm_cfgi_base_active_t;
OBJ_CLASS_DECLARATION(orcm_cfgi_base_active_t);

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_cfgi_base_framework;
/*
 * Select available components.
 */
ORCM_DECLSPEC int orcm_cfgi_base_select(void);

/* Public APIs */
ORCM_DECLSPEC int orcm_cfgi_base_read_config(opal_list_t *config);
ORCM_DECLSPEC int orcm_cfgi_base_define_sys(opal_list_t *config,
                                            orcm_node_t **mynode,
                                            orte_vpid_t *num_procs,
                                            opal_buffer_t *buf);


/* parsing support */
ORCM_DECLSPEC int orcm_cfgi_base_parse_cluster(orcm_cluster_t *cluster,
                                               orcm_cfgi_xml_parser_t *x);
ORCM_DECLSPEC void orcm_cfgi_base_construct_uri(opal_buffer_t *buf, orcm_node_t *node);
ORCM_DECLSPEC int orcm_cfgi_base_identify_emulator_targets(opal_list_t *targets);
ORCM_DECLSPEC int orcm_cfgi_base_get_children(orte_process_name_t *proc, char **noderegex);
ORCM_DECLSPEC int orcm_cfgi_base_get_proc_hostname(orte_process_name_t *proc, char **hostname);
ORCM_DECLSPEC int orcm_cfgi_base_get_hostname_proc(char *hostname, orte_process_name_t *proc);

/* datatype support */
ORCM_DECLSPEC int orcm_pack_node(opal_buffer_t *buffer, const void *src,
                                 int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_node(opal_buffer_t *buffer, void *dest,
                                   int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_node(orcm_node_t *value1,
                                    orcm_node_t *value2,
                                    opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_node(orcm_node_t **dest,
                                 orcm_node_t *src,
                                 opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_node(char **output, char *prefix,
                                  orcm_node_t *src, opal_data_type_t type);

ORCM_DECLSPEC int orcm_pack_rack(opal_buffer_t *buffer, const void *src,
                                 int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_rack(opal_buffer_t *buffer, void *dest,
                                   int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_rack(orcm_rack_t *value1,
                                    orcm_rack_t *value2,
                                    opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_rack(orcm_rack_t **dest,
                                 orcm_rack_t *src,
                                 opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_rack(char **output, char *prefix,
                                  orcm_rack_t *src, opal_data_type_t type);

ORCM_DECLSPEC int orcm_pack_row(opal_buffer_t *buffer, const void *src,
                                int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_row(opal_buffer_t *buffer, void *dest,
                                  int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_row(orcm_row_t *value1,
                                   orcm_row_t *value2,
                                   opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_row(orcm_row_t **dest,
                                orcm_row_t *src,
                                opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_row(char **output, char *prefix,
                                 orcm_row_t *src, opal_data_type_t type);

ORCM_DECLSPEC int orcm_pack_cluster(opal_buffer_t *buffer, const void *src,
                                    int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_cluster(opal_buffer_t *buffer, void *dest,
                                      int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_cluster(orcm_cluster_t *value1,
                                       orcm_cluster_t *value2,
                                       opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_cluster(orcm_cluster_t **dest,
                                    orcm_cluster_t *src,
                                    opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_cluster(char **output, char *prefix,
                                     orcm_cluster_t *src, opal_data_type_t type);

ORCM_DECLSPEC int orcm_pack_scheduler(opal_buffer_t *buffer, const void *src,
                                      int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_scheduler(opal_buffer_t *buffer, void *dest,
                                        int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_scheduler(orcm_scheduler_t *value1,
                                         orcm_scheduler_t *value2,
                                         opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_scheduler(orcm_scheduler_t **dest,
                                      orcm_scheduler_t *src,
                                      opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_scheduler(char **output, char *prefix,
                                       orcm_scheduler_t *src, opal_data_type_t type);

END_C_DECLS
#endif
