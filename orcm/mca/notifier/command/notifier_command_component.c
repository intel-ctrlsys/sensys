/* -*- C -*-
 *
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2009      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * Simple command notifier
 */

#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>

#include "opal/mca/base/mca_base_var.h"
#include "opal/util/argv.h"

#include "orcm/constants.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"

#include "notifier_command.h"

static int command_component_query(mca_base_module_t **module, int *priority);
static int command_open(void);
static int command_close(void);
static int command_register(void);


/*
 * Struct of function pointers that need to be initialized
 */
orcm_notifier_command_component_t mca_notifier_command_component = {
    {
        {
            ORCM_NOTIFIER_BASE_VERSION_1_0_0,
            
            "command",
            
            ORCM_MAJOR_VERSION,
            ORCM_MINOR_VERSION,
            ORCM_RELEASE_VERSION,
            
            command_open,
            command_close,
            command_component_query,
            command_register,
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    },
};

/* Safety to ensure we don't try to write down a dead pipe */
static void child_death_cb(pid_t pid, int status, void *data)
{
    if (pid == mca_notifier_command_component.child_pid) {
        OPAL_OUTPUT((0, "Command notifier: child unexpectedly died!  Exited, %d, exitstatus %d", WIFEXITED(status), WEXITSTATUS(status)));
        mca_notifier_command_component.child_pid = 0;        
        mca_notifier_command_component.to_child[1] = -1;
    }
}

static int command_open(void)
{
    mca_notifier_command_component.child_pid = -1;
    mca_notifier_command_component.to_child[0] = -1;
    mca_notifier_command_component.to_child[1] = -1;
    mca_notifier_command_component.to_parent[0] = -1;
    mca_notifier_command_component.to_parent[1] = -1;
    return ORCM_SUCCESS;
}

static int command_register(void)
{
    mca_notifier_command_component.cmd = strdup("/sbin/initlog -f $s -n \"Open MPI\" -s \"$S: $m (errorcode: $e)\"");
    (void) mca_base_component_var_register(&mca_notifier_command_component.super.base_version, "cmd",
                                           "Command to execute, with substitution.  $s = integer severity; $S = string severity; $e = integer error code; $m = string message",
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_command_component.cmd);
    
    mca_notifier_command_component.timeout= 30;
    (void) mca_base_component_var_register(&mca_notifier_command_component.super.base_version, "timeout",
                                           "Timeout (in seconds) of the command",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_command_component.timeout);

    mca_notifier_command_component.priority= 10;
    (void) mca_base_component_var_register(&mca_notifier_command_component.super.base_version, "timeout",
                                           "Priority of this component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_command_component.priority);

    mca_notifier_command_component.pass_via_stdin = false;
    (void) mca_base_component_var_register(&mca_notifier_command_component.super.base_version, "timeout",
                                           "If true, pass parameters to the command via stdin, formatted with trivial XML",
                                           MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_command_component.pass_via_stdin);

    return ORCM_SUCCESS;
}


static int command_close(void)
{
    if (NULL != mca_notifier_command_component.cmd) {
        free(mca_notifier_command_component.cmd);
    }

    /* Tell the child process to die */
    if (0 != mca_notifier_command_component.child_pid &&
        -1 != mca_notifier_command_component.to_child[1]) {
        orcm_notifier_command_pipe_cmd_t cmd = CMD_TIME_TO_QUIT;
        orcm_notifier_command_write_fd(mca_notifier_command_component.to_child[1],
                                     sizeof(cmd), &cmd);

        close(mca_notifier_command_component.to_child[1]);
        mca_notifier_command_component.to_child[1] = -1;

        close(mca_notifier_command_component.to_parent[0]);
        mca_notifier_command_component.to_parent[0] = -1;
    }


    return ORCM_SUCCESS;
}

static int command_component_query(mca_base_module_t **module, int *priority)
{
    char **argv = NULL;

    *priority = 0;
    *module = NULL;

    /* If there's no command, there's no love */
    if (NULL == mca_notifier_command_component.cmd ||
        '\0' == mca_notifier_command_component.cmd[0]) {
        orte_show_help("help-orcm-notifier-command.txt", 
                       "command not specified", true);
        return ORCM_ERR_NOT_FOUND;
    }

    /* Attempt to parse the command into argv, just as a basic sanity
       check to ensure that it seems to be ok. */
    if (ORCM_SUCCESS != 
        orcm_notifier_command_split(mca_notifier_command_component.cmd, &argv)) {
        orte_show_help("help-orcm-notifier-command.txt", 
                       "bad command", true, orte_process_info.nodename,
                       mca_notifier_command_component.cmd);
        return ORCM_ERR_BAD_PARAM;
    }
    opal_argv_free(argv);

    /* Create the pipe to be used (it'll be closed in component
       close if we're not selected) */
    if (0 != pipe(mca_notifier_command_component.to_child) ||
        0 != pipe(mca_notifier_command_component.to_parent)) {
        int save = errno;
        orte_show_help("help-orcm-notifier-command.txt", 
                       "system call fail", true, orte_process_info.nodename,
                       "pipe", save, strerror(save));
        errno = save;
        return ORCM_ERR_IN_ERRNO;
    }

    /* Create the child (it'll be shut down in component close if
       we're not selected).  We create the child very early so that we
       do it before any MPI networks are initialized that have
       problems with fork().  The child sits on the other end of a
       pipe and waits for commands from this main process.  Commands
       include telling the child to fork/exec a proces and shutting
       down. */
    mca_notifier_command_component.child_pid = fork();
    if (mca_notifier_command_component.child_pid < 0) {
        int save = errno;
        orte_show_help("help-orcm-notifier-command.txt", 
                       "system call fail", true, orte_process_info.nodename,
                       "fork", save, strerror(save));
        errno = save;
        return ORCM_ERR_IN_ERRNO;
    }

    /* Child: close all fd's except the reading pipe and call the
       child main routine */
    if (0 == mca_notifier_command_component.child_pid) {
        int i;
        int fdmax = sysconf(_SC_OPEN_MAX);
        for (i = 3; i < fdmax; ++i) {
            if (i != mca_notifier_command_component.to_child[0] &&
                i != mca_notifier_command_component.to_parent[1]) {
                close(i);
            }
        }

        orcm_notifier_command_child_main();
        /* Never returns */
    }

    /* Parent: close other ends of pipes */
    close(mca_notifier_command_component.to_child[0]);
    close(mca_notifier_command_component.to_parent[1]);

    /* Let's find out if the child unexpectedly dies */
    // orte_wait_cb(mca_notifier_command_component.child_pid, child_death_cb, 0);

    *priority = 10;
    *module = (mca_base_module_t *) &orcm_notifier_command_module;
    return ORCM_SUCCESS;    
}
