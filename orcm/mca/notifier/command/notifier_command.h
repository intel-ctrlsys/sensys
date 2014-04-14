/* -*- C -*-
 * 
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */
#ifndef NOTIFIER_COMMAND_H
#define NOTIFIER_COMMAND_H

#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "orcm/mca/notifier/notifier.h"

BEGIN_C_DECLS

typedef struct {
    orcm_notifier_base_component_t super;

    /* Command to execute */
    char *cmd;

    /* Timeout of the command (seconds) */
    int timeout;

    /* Priority of this component */
    int priority;

    /* Child PID */
    pid_t child_pid;

    /* Pipe to the child */
    int to_child[2];

    /* Pipe to the parent */
    int to_parent[2];

    /* Do we want data sent to child via stdin? */
    bool pass_via_stdin;
} orcm_notifier_command_component_t;


/*
 * Notifier interfaces
 */
ORCM_MODULE_DECLSPEC extern orcm_notifier_command_component_t 
    mca_notifier_command_component;
extern orcm_notifier_base_module_t orcm_notifier_command_module;

/*
 * Pipe commands
 */
typedef enum {
    /* Fork/exec a command */
    CMD_EXEC,

    /* Time to quit */
    CMD_TIME_TO_QUIT,

    /* Sentinel value */
    CMD_MAX
} orcm_notifier_command_pipe_cmd_t;


/**
 * Simple blocking function to read a specific number of bytes from an
 * fd.
 */
int orcm_notifier_command_read_fd(int fd, int len, void *buffer);

/**
 * Simple blocking function to write a specific number of bytes to an
 * fd.
 */
int orcm_notifier_command_write_fd(int fd, int len, void *buffer);

/**
 * Main entry point for child
 */
void orcm_notifier_command_child_main(void) __opal_attribute_noreturn__;

/**
 * Function to split a spint into argv, honoring quoting, etc. (and do
 * some error checking of the string)
 */
int orcm_notifier_command_split(const char *cmd, char ***argv);

END_C_DECLS

#endif
