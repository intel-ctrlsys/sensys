/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCMAPI_H
#define ORCMAPI_H

BEGIN_C_DECLS

typedef struct {
    char *name;
    orcm_node_state_t state;
} liborcm_node_t;

/******************
 * API Functions
 ******************/
/* Initialize the ORCMAPI library */
ORCM_DECLSPEC int orcmapi_init(void);

/* Finalize the ORCMAPI library */
ORCM_DECLSPEC void orcmapi_finalize(void);

/* get node info/states */
ORCM_DECLSPEC int orcmapi_get_nodes(liborcm_node_t ***nodes, int *count);
/* launch session */
ORCM_DECLSPEC int orcmapi_launch_session(int id, int min_nodes, char *nodes, char *user);
/* cancel session */
ORCM_DECLSPEC int orcmapi_cancel_session(int id);

END_C_DECLS

#endif /* ORTED_H */
