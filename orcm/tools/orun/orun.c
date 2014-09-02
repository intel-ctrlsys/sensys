/* -*- C -*-
 *
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2008 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2009 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2007-2013 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif  /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */
#include <fcntl.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "opal/mca/event/event.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/hwloc/base/base.h"
#include "opal/mca/base/base.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/opal_getcwd.h"
#include "opal/util/show_help.h"
#include "opal/sys/atomic.h"
#if OPAL_ENABLE_FT_CR == 1
#include "opal/runtime/opal_cr.h"
#endif

#include "opal/version.h"
#include "opal/runtime/opal.h"
#include "opal/util/os_path.h"
#include "opal/util/path.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/dss/dss.h"

#include "orte/util/proc_info.h"
#include "orte/util/pre_condition_transports.h"
#include "orte/util/session_dir.h"
#include "orte/util/hnp_contact.h"
#include "orte/util/show_help.h"

#include "orte/mca/dfs/dfs.h"
#include "orte/mca/odls/odls.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/rmaps/rmaps_types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/errmgr/base/errmgr_private.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/state/state.h"

#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_data_server.h"
#include "orte/runtime/orte_locks.h"
#include "orte/runtime/orte_quit.h"

#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/scd/scd_types.h"
#include "orcm/runtime/runtime.h"

/* ensure I can behave like a daemon */
#include "orte/orted/orted.h"

#include "orun.h"

/*
 * Globals
 */
static char **global_mca_env = NULL;
static orte_std_cntr_t total_num_apps = 0;
static bool want_prefix_by_default = (bool) ORTE_WANT_ORTERUN_PREFIX_BY_DEFAULT;
static char *ompi_server=NULL;
static char *my_hnp_uri=NULL;
struct orun_globals_t orun_globals;
static bool globals_init = false;

static opal_cmd_line_init_t cmd_line_init[] = {
    /* Various "obvious" options */
    { NULL, 'h', NULL, "help", 0,
      &orun_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },
    { NULL, 'V', NULL, "version", 0,
      &orun_globals.version, OPAL_CMD_LINE_TYPE_BOOL,
      "Print version and exit" },

    /* Number of processes; -np, and --np are all
       synonyms */
    { NULL, '\0', "np", "np", 1,
      &orun_globals.num_procs, OPAL_CMD_LINE_TYPE_INT,
      "Number of processes to run" },
    
    { "rmaps_ppr_n_pernode", '\0', "N", NULL, 1,
        NULL, OPAL_CMD_LINE_TYPE_INT,
        "Launch n processes per node on all allocated nodes (synonym for npernode)" },
    { NULL, '\0', "path", "path", 1,
      &orun_globals.path, OPAL_CMD_LINE_TYPE_STRING,
      "PATH to be used to look for executables to start processes" },

    /* OpenRTE arguments */
    { "orte_debug", 'd', "debug", "debug", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE" },
    
    { "orte_debug_daemons", '\0', "debug-daemons", "debug-daemons", 0,
      NULL, OPAL_CMD_LINE_TYPE_INT,
      "Enable debugging of any OpenRTE daemons used by this application" },
    
    { "orte_debug_daemons_file", '\0', "debug-daemons-file", "debug-daemons-file", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of any OpenRTE daemons used by this application, storing output in files" },

    /* Use an appfile */
    { NULL, '\0', NULL, "app", 1,
      &orun_globals.appfile, OPAL_CMD_LINE_TYPE_STRING,
      "Provide an appfile; ignore all other command line options" },
    
    /* Scheduler options for new allocation */
    { NULL,
      '\0', NULL, "account",
      1,
      &orun_globals.account, OPAL_CMD_LINE_TYPE_STRING,
      "Account to be charged" },

    { NULL,
      '\0', NULL, "project",
      1,
      &orun_globals.name, OPAL_CMD_LINE_TYPE_STRING,
      "User assigned project name" },

    { NULL,
      '\0', NULL, "gid",
      1,
      &orun_globals.gid, OPAL_CMD_LINE_TYPE_INT,
      "Group id to run session under" },

    { NULL,
      '\0', NULL, "max-node",
      1,
      &orun_globals.max_nodes, OPAL_CMD_LINE_TYPE_INT,
      "Max nodes allowed in allocation" },

    { NULL,
      '\0', NULL, "min-node",
      1,
      &orun_globals.min_nodes, OPAL_CMD_LINE_TYPE_INT,
      "Minimum number of nodes required for allocation" },

    { NULL,
      '\0', NULL, "node",
      1,
      &orun_globals.min_nodes, OPAL_CMD_LINE_TYPE_INT,
      "Minimum number of nodes required for allocation" },

    {"orte_hnp_uri", '\0', "hnp-uri", "hnp-uri", 1,
      &my_hnp_uri, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the HNP"},

    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }

};

/*
 * Local functions
 */
static int create_app(int argc, char* argv[],
                      orte_job_t *jdata,
                      orte_app_context_t **app,
                      bool *made_app, char ***app_env);
static int init_globals(void);
static int parse_globals(int argc, char* argv[], opal_cmd_line_t *cmd_line);
static int parse_locals(orte_job_t *jdata, int argc, char* argv[]);
static int parse_appfile(orte_job_t *jdata, char *filename, char ***env);
static int init_sched_args(void);
static int parse_args_sched(int argc, char *argv[], opal_cmd_line_t *cmd_line);
static int alloc_request( orcm_alloc_t *alloc );

int orun(int argc, char *argv[])
{
    int rc, n;
    opal_cmd_line_t cmd_line;
    char *param=NULL;
    orte_job_t *jdata=NULL;
    orcm_alloc_t alloc;
    orcm_rm_cmd_flag_t command;
    char *hnp_uri;
    orte_rml_recv_cb_t xbuffer;

    /* find our basename (the name of the executable) so that we can
       use it in pretty-print error messages */
    orte_basename = opal_basename(argv[0]);

    /* bozo check - we don't allow recursive calls of orun */
    if (NULL != getenv("OMPI_UNIVERSE_SIZE")) {
        fprintf(stderr, "\n\n**********************************************************\n\n");
        fprintf(stderr, "Open MPI does not support recursive calls of %s\n", orte_basename);
        fprintf(stderr, "\n**********************************************************\n");
        exit(1);
    }

    /* Setup and parse the command line */
    init_globals();
    init_sched_args();
    opal_cmd_line_create(&cmd_line, cmd_line_init);
    mca_base_cmd_line_setup(&cmd_line);
    if (OPAL_SUCCESS != (rc = opal_cmd_line_parse(&cmd_line, true,
                                                  argc, argv)) ) {
        if (OPAL_ERR_SILENT != rc) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(rc));
        }
        return rc;
    }

    /* NOTE: (JJH)
     *  We need to allow 'mca_base_cmd_line_process_args()' to process command
     *  line arguments *before* calling opal_init_util() since the command
     *  line could contain MCA parameters that affect the way opal_init_util()
     *  functions. AMCA parameters are one such option normally received on the
     *  command line that affect the way opal_init_util() behaves.
     *  It is "safe" to call mca_base_cmd_line_process_args() before 
     *  opal_init_util() since mca_base_cmd_line_process_args() does *not*
     *  depend upon opal_init_util() functionality.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);
    
    /* Need to initialize OPAL so that install_dirs are filled in */
    if (OPAL_SUCCESS != opal_init_util(&argc, &argv)) {
        exit(1);
    }
    
    /* may look strange, but the way we handle prefix is a little weird
     * and probably needs to be addressed more fully at some future point.
     * For now, we have a conflict between app_files and cmd line usage.
     * Since app_files are used by the C/R system, we will make an
     * adjustment here to avoid perturbing that system.
     *
     * We cannot just have the cmd line parser place any found value
     * in the global struct as the app_file parser would replace it.
     * So handle this specific cmd line option manually.
     */
    orun_globals.prefix = NULL;
    orun_globals.path_to_mpirun = NULL;
    if (opal_cmd_line_is_taken(&cmd_line, "prefix") ||
        '/' == argv[0][0] || want_prefix_by_default) {
        size_t param_len;
        if ('/' == argv[0][0]) {
            char* tmp_basename = NULL;
            /* If they specified an absolute path, strip off the
               /bin/<exec_name>" and leave just the prefix */
            orun_globals.path_to_mpirun = opal_dirname(argv[0]);
            /* Quick sanity check to ensure we got
               something/bin/<exec_name> and that the installation
               tree is at least more or less what we expect it to
               be */
            tmp_basename = opal_basename(orun_globals.path_to_mpirun);
            if (0 == strcmp("bin", tmp_basename)) {
                char* tmp = orun_globals.path_to_mpirun;
                orun_globals.path_to_mpirun = opal_dirname(tmp);
                free(tmp);
            } else {
                free(orun_globals.path_to_mpirun);
                orun_globals.path_to_mpirun = NULL;
            }
            free(tmp_basename);
        }
        /* if both are given, check to see if they match */
        if (opal_cmd_line_is_taken(&cmd_line, "prefix") && NULL != orun_globals.path_to_mpirun) {
            char *tmp_basename;
            char *cli_param;
            /* if they don't match, then that merits a warning */
            cli_param = opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0);
            if ( NULL != cli_param ) {
                param = strdup(cli_param);
            }
            /* ensure we strip any trailing '/' */
            if (NULL != param && 0 == strcmp(OPAL_PATH_SEP, &(param[strlen(param)-1]))) {
                param[strlen(param)-1] = '\0';
            }
            tmp_basename = strdup(orun_globals.path_to_mpirun);
            if (0 == strcmp(OPAL_PATH_SEP, &(tmp_basename[strlen(tmp_basename)-1]))) {
                tmp_basename[strlen(tmp_basename)-1] = '\0';
            }
            if (NULL != param && 0 != strcmp(param, tmp_basename)) {
                orte_show_help("help-orun.txt", "orun:double-prefix",
                               true, orte_basename, orte_basename,
                               param, tmp_basename, orte_basename);
                /* use the prefix over the path-to-mpirun so that
                 * people can specify the backend prefix as different
                 * from the local one
                 */
                free(orun_globals.path_to_mpirun);
                orun_globals.path_to_mpirun = NULL;
            }
            free(tmp_basename);
            free(cli_param);
        } else if (NULL != orun_globals.path_to_mpirun) {
            param = orun_globals.path_to_mpirun;
        } else if (opal_cmd_line_is_taken(&cmd_line, "prefix")){
            /* must be --prefix alone */
            char *cli_param;
            cli_param = opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0);
            if ( NULL != cli_param ) {
                param = strdup(cli_param);
            }
            free(cli_param);
        } else {
            /* --enable-orun-prefix-default was given to orun */
            param = strdup(opal_install_dirs.prefix);
        }

        if (NULL != param) {
            /* "Parse" the param, aka remove superfluous path_sep. */
            param_len = strlen(param);
            while (0 == strcmp (OPAL_PATH_SEP, &(param[param_len-1]))) {
                param[param_len-1] = '\0';
                param_len--;
                if (0 == param_len) {
                    orte_show_help("help-orun.txt", "orun:empty-prefix",
                                   true, orte_basename, orte_basename);
                    free(param);
                    return ORTE_ERR_FATAL;
                }
            }

            orun_globals.prefix = strdup(param);
            free(param);
        }
        want_prefix_by_default = true;
    }

    /* Check for some "global" command line params */
    parse_globals(argc, argv, &cmd_line);
    /* check for new allocation request */
    parse_args_sched(argc, argv, &cmd_line);

    OBJ_DESTRUCT(&cmd_line);

    /* create a new job object to hold the info for this one - the
     * jobid field will be filled in by the PLM when the job is
     * launched
     */
    jdata = OBJ_NEW(orte_job_t);
    if (NULL == jdata) {
        /* cannot call ORTE_ERROR_LOG as the errmgr
         * hasn't been loaded yet!
         */
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
    
    /* add a map object */
    jdata->map = OBJ_NEW(orte_job_map_t);

    /* check what user wants us to do with stdin */
    if (0 == strcmp(orun_globals.stdin_target, "all")) {
        jdata->stdin_target = ORTE_VPID_WILDCARD;
    } else if (0 == strcmp(orun_globals.stdin_target, "none")) {
        jdata->stdin_target = ORTE_VPID_INVALID;
    } else {
        jdata->stdin_target = strtoul(orun_globals.stdin_target, NULL, 10);
    }
    
    /* Parse each app, adding it to the job object */
    parse_locals(jdata, argc, argv);
    
    if (0 == jdata->num_apps) {
        /* This should never happen -- this case should be caught in
           create_app(), but let's just double check... */
        orte_show_help("help-orun.txt", "orun:nothing-to-do",
                       true, orte_basename);
        exit(ORTE_ERROR_DEFAULT_EXIT_CODE);
    }

    /* Intialize ORCM */
    if (ORCM_SUCCESS != (rc = orcm_init(ORCM_TOOL))) {
        /* cannot call ORTE_ERROR_LOG as it could be the errmgr
         * never got loaded!
         */
        return rc;
    }

    /* setup my session directory */
    orte_create_session_dirs = false;

    /* finalize the OPAL utils. As they are opened again from orte_init->opal_init
     * we continue to have a reference count on them. So we have to finalize them twice...
     */
    opal_finalize_util();

    /* pre-condition any network transports that require it */
    if (ORTE_SUCCESS != (rc = orte_pre_condition_transports(jdata))) {
        ORTE_ERROR_LOG(rc);
        orte_show_help("help-orun.txt", "orun:precondition", false,
                       orte_basename, NULL, NULL, rc);
        ORTE_UPDATE_EXIT_STATUS(ORTE_ERROR_DEFAULT_EXIT_CODE);
        goto DONE;
    }

    /* setup to listen for commands sent specifically to me, even though I would probably
     * be the one sending them! Unfortunately, since I am a participating daemon,
     * there are times I need to send a command to "all daemons", and that means *I* have
     * to receive it too
     */
    /* setup to receive the result */
    OBJ_CONSTRUCT(&xbuffer, orte_rml_recv_cb_t);
    xbuffer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_TOOL,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xbuffer);
    
    if (!my_hnp_uri && (false == orun_globals.alloc_request)) {
        orte_show_help("help-orun.txt", "orun:allocation-not-specified",
                       false, orte_basename, orte_basename);
        rc = ORCM_ERR_BAD_PARAM;
        ORTE_ERROR_LOG(rc);
        goto DONE;
    }

    if(my_hnp_uri) {
        /* set the contact info to the hash table */
        orte_rml.set_contact_info(my_hnp_uri);
        rc = orte_rml_base_parse_uris(my_hnp_uri, ORTE_PROC_MY_HNP, NULL);
        if (ORTE_SUCCESS != rc) {
            ORTE_ERROR_LOG(rc);
            goto DONE;
        }

    }

    /* create the allocation object */
    if ( true == orun_globals.alloc_request ) {
        OBJ_CONSTRUCT(&alloc, orcm_alloc_t);
        if (ORTE_SUCCESS != (rc = alloc_request(&alloc))) {
            ORTE_ERROR_LOG(rc);
            orte_show_help("help-orun.txt", "orun:alloc_request", false,
                       orte_basename, NULL, NULL, rc);
            ORTE_UPDATE_EXIT_STATUS(ORTE_ERROR_DEFAULT_EXIT_CODE);
            goto DONE;
        }

        ORTE_WAIT_FOR_COMPLETION(xbuffer.active);
        /* unpack the command */
        n = 1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&xbuffer.data, &command, &n, ORCM_RM_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            goto DONE;
         }

         /* now process the command locally */
         switch(command) {

         case ORCM_VM_READY_COMMAND:
             n = 1;
             if (ORTE_SUCCESS != (rc = opal_dss.unpack(&xbuffer.data, &hnp_uri, &n, OPAL_STRING))) {
                 ORTE_ERROR_LOG(rc);
                 goto DONE;
             }
             orte_rml.set_contact_info(hnp_uri);
             rc = orte_rml_base_parse_uris(hnp_uri, ORTE_PROC_MY_HNP, NULL);
             if (ORTE_SUCCESS != rc) {
                 ORTE_ERROR_LOG(rc);
                 goto DONE;
             }
             /* spawn the job and its daemons */
             break;

         default:
            ORTE_UPDATE_EXIT_STATUS(ORTE_ERROR_DEFAULT_EXIT_CODE);
             goto DONE;
         }

    }
            
    /* spawn the job and its daemons */
    rc = orte_plm.spawn(jdata);

    /* loop the event lib until an exit event is detected */
    while (orte_event_base_active) {
        opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
    }

    /* ensure all local procs are dead */
    orte_odls.kill_local_procs(NULL);

 DONE:
    /* cleanup and leave */
    orte_finalize();

    if (orte_debug_flag) {
        fprintf(stderr, "exiting with status %d\n", orte_exit_status);
    }
    exit(orte_exit_status);
}

static int init_globals(void)
{
    /* Only CONSTRUCT things once */
    if (!globals_init) {
        orun_globals.env_val =     NULL;
        orun_globals.appfile =     NULL;
        orun_globals.path =        NULL;
        orun_globals.stdin_target = "0";
    }

    /* Reset the other fields every time */

    orun_globals.help                       = false;
    orun_globals.version                    = false;
    orun_globals.verbose                    = false;
    orun_globals.debugger                   = false;
    orun_globals.num_procs                  =  0;
    if( NULL != orun_globals.env_val )
        free( orun_globals.env_val );
    orun_globals.env_val =     NULL;
    if( NULL != orun_globals.appfile )
        free( orun_globals.appfile );
    orun_globals.appfile =     NULL;
    if( NULL != orun_globals.path )
        free( orun_globals.path );
    orun_globals.path =        NULL;

#if OPAL_ENABLE_FT_CR == 1
    orun_globals.sstore_load = NULL;
#endif

    /* All done */
    globals_init = true;
    return ORTE_SUCCESS;
}


static int parse_globals(int argc, char* argv[], opal_cmd_line_t *cmd_line)
{
    /* print version if requested.  Do this before check for help so
       that --version --help works as one might expect. */
    if (orun_globals.version) {
        char *str, *project_name = "orun";
        str = opal_show_help_string("help-orun.txt", "orun:version", 
                                    false,
                                    orte_basename, project_name, OPAL_VERSION,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        exit(0);
    }

    /* Check for help request */
    if (orun_globals.help) {
        char *str, *args = NULL;
        char *project_name = "orun";
        args = opal_cmd_line_get_usage_msg(cmd_line);
        str = opal_show_help_string("help-orun.txt", "orun:usage", false,
                                    orte_basename, project_name, OPAL_VERSION,
                                    orte_basename, args,
                                    PACKAGE_BUGREPORT);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);

        /* If someone asks for help, that should be all we do */
        exit(0);
    }

    return ORTE_SUCCESS;
}


static int parse_locals(orte_job_t *jdata, int argc, char* argv[])
{
    int i, rc, app_num;
    int temp_argc;
    char **temp_argv, **env;
    orte_app_context_t *app;
    bool made_app;
    orte_std_cntr_t j, size1;

    /* Make the apps */

    temp_argc = 0;
    temp_argv = NULL;
    opal_argv_append(&temp_argc, &temp_argv, argv[0]);

    /* NOTE: This bogus env variable is necessary in the calls to
       create_app(), below.  See comment immediately before the
       create_app() function for an explanation. */

    env = NULL;
    for (app_num = 0, i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], ":")) {
            /* Make an app with this argv */
            if (opal_argv_count(temp_argv) > 1) {
                if (NULL != env) {
                    opal_argv_free(env);
                    env = NULL;
                }
                app = NULL;
                rc = create_app(temp_argc, temp_argv, jdata, &app, &made_app, &env);
                /** keep track of the number of apps - point this app_context to that index */
                if (ORTE_SUCCESS != rc) {
                    /* Assume that the error message has already been
                       printed; no need to cleanup -- we can just
                       exit */
                    exit(1);
                }
                if (made_app) {
                    app->idx = app_num;
                    ++app_num;
                    opal_pointer_array_add(jdata->apps, app);
                    ++jdata->num_apps;
                }

                /* Reset the temps */

                temp_argc = 0;
                temp_argv = NULL;
                opal_argv_append(&temp_argc, &temp_argv, argv[0]);
            }
        } else {
            opal_argv_append(&temp_argc, &temp_argv, argv[i]);
        }
    }

    if (opal_argv_count(temp_argv) > 1) {
        app = NULL;
        rc = create_app(temp_argc, temp_argv, jdata, &app, &made_app, &env);
        if (ORTE_SUCCESS != rc) {
            /* Assume that the error message has already been printed;
               no need to cleanup -- we can just exit */
            exit(1);
        }
        if (made_app) {
            app->idx = app_num;
            ++app_num;
            opal_pointer_array_add(jdata->apps, app);
            ++jdata->num_apps;
        }
    }
    if (NULL != env) {
        opal_argv_free(env);
    }
    opal_argv_free(temp_argv);

    /* Once we've created all the apps, add the global MCA params to
       each app's environment (checking for duplicates, of
       course -- yay opal_environ_merge()).  */

    if (NULL != global_mca_env) {
        size1 = (size_t)opal_pointer_array_get_size(jdata->apps);
        /* Iterate through all the apps */
        for (j = 0; j < size1; ++j) {
            app = (orte_app_context_t *)
                opal_pointer_array_get_item(jdata->apps, j);
            if (NULL != app) {
                /* Use handy utility function */
                env = opal_environ_merge(global_mca_env, app->env);
                opal_argv_free(app->env);
                app->env = env;
            }
        }
    }

    /* Now take a subset of the MCA params and set them as MCA
       overrides here in orun (so that when we orte_init() later,
       all the components see these MCA params).  Here's how we decide
       which subset of the MCA params we set here in orun:

       1. If any global MCA params were set, use those
       2. If no global MCA params were set and there was only one app,
          then use its app MCA params
       3. Otherwise, don't set any
    */

    env = NULL;
    if (NULL != global_mca_env) {
        env = global_mca_env;
    } else {
        if (opal_pointer_array_get_size(jdata->apps) >= 1) {
            /* Remember that pointer_array's can be padded with NULL
               entries; so only use the app's env if there is exactly
               1 non-NULL entry */
            app = (orte_app_context_t *)
                opal_pointer_array_get_item(jdata->apps, 0);
            if (NULL != app) {
                env = app->env;
                for (j = 1; j < opal_pointer_array_get_size(jdata->apps); ++j) {
                    if (NULL != opal_pointer_array_get_item(jdata->apps, j)) {
                        env = NULL;
                        break;
                    }
                }
            }
        }
    }

    if (NULL != env) {
        size1 = opal_argv_count(env);
        for (j = 0; j < size1; ++j) {
            /* Use-after-Free error possible here.  putenv does not copy
             * the string passed to it, and instead stores only the pointer.
             * env[j] may be freed later, in which case the pointer
             * in environ will now be left dangling into a deallocated
             * region.
             * So we make a copy of the variable.
             */
            char *s = strdup(env[j]);
            
            if (NULL == s) {
                return OPAL_ERR_OUT_OF_RESOURCE;
            }
            putenv(s);
        }
    }

    /* All done */

    return ORTE_SUCCESS;
}


static int capture_cmd_line_params(int argc, int start, char **argv)
{
    int i, j, k;
    bool ignore;
    char *no_dups[] = {
        "grpcomm",
        "odls",
        "rml",
        "routed",
        NULL
    };
    
    for (i = 0; i < (argc-start); ++i) {
        if (0 == strcmp("-mca",  argv[i]) ||
            0 == strcmp("--mca", argv[i]) ) {
            /* It would be nice to avoid increasing the length
             * of the orted cmd line by removing any non-ORTE
             * params. However, this raises a problem since
             * there could be OPAL directives that we really
             * -do- want the orted to see - it's only the OMPI
             * related directives we could ignore. This becomes
             * a very complicated procedure, however, since
             * the OMPI mca params are not cleanly separated - so
             * filtering them out is nearly impossible.
             *
             * see if this is already present so we at least can
             * avoid growing the cmd line with duplicates
             */
            ignore = false;
            if (NULL != orted_cmd_line) {
                for (j=0; NULL != orted_cmd_line[j]; j++) {
                    if (0 == strcmp(argv[i+1], orted_cmd_line[j])) {
                        /* already here - if the value is the same,
                         * we can quitely ignore the fact that they
                         * provide it more than once. However, some
                         * frameworks are known to have problems if the
                         * value is different. We don't have a good way
                         * to know this, but we at least make a crude
                         * attempt here to protect ourselves.
                         */
                        if (0 == strcmp(argv[i+2], orted_cmd_line[j+1])) {
                            /* values are the same */
                            ignore = true;
                            break;
                        } else {
                            /* values are different - see if this is a problem */
                            for (k=0; NULL != no_dups[k]; k++) {
                                if (0 == strcmp(no_dups[k], argv[i+1])) {
                                    /* print help message
                                     * and abort as we cannot know which one is correct
                                     */
                                    orte_show_help("help-orun.txt", "orun:conflicting-params",
                                                   true, orte_basename, argv[i+1],
                                                   argv[i+2], orted_cmd_line[j+1]);
                                    return ORTE_ERR_BAD_PARAM;
                                }
                            }
                            /* this passed muster - just ignore it */
                            ignore = true;
                            break;
                        }
                    }
                }
            }
            if (!ignore) {
                opal_argv_append_nosize(&orted_cmd_line, argv[i]);
                opal_argv_append_nosize(&orted_cmd_line, argv[i+1]);
                opal_argv_append_nosize(&orted_cmd_line, argv[i+2]);
            }
            i += 2;
        }
    }
    
    return ORTE_SUCCESS;
}


/*
 * This function takes a "char ***app_env" parameter to handle the
 * specific case:
 *
 *   orun --mca foo bar -app appfile
 *
 * That is, we'll need to keep foo=bar, but the presence of the app
 * file will cause an invocation of parse_appfile(), which will cause
 * one or more recursive calls back to create_app().  Since the
 * foo=bar value applies globally to all apps in the appfile, we need
 * to pass in the "base" environment (that contains the foo=bar value)
 * when we parse each line in the appfile.
 *
 * This is really just a special case -- when we have a simple case like:
 *
 *   orun --mca foo bar -np 4 hostname
 *
 * Then the upper-level function (parse_locals()) calls create_app()
 * with a NULL value for app_env, meaning that there is no "base"
 * environment that the app needs to be created from.
 */
static int create_app(int argc, char* argv[],
                      orte_job_t *jdata,
                      orte_app_context_t **app_ptr,
                      bool *made_app, char ***app_env)
{
    opal_cmd_line_t cmd_line;
    char cwd[OPAL_PATH_MAX];
    int i, j, count, rc;
    char *param = NULL, *value, *value2;
    orte_app_context_t *app = NULL;
    bool cmd_line_made = false;
    bool found = false;
    char *appname;

    *made_app = false;

    /* Pre-process the command line if we are going to parse an appfile later.
     * save any mca command line args so they can be passed
     * separately to the daemons.
     * Use Case:
     *  $ cat launch.appfile
     *  -np 1 -mca aaa bbb ./my-app -mca ccc ddd
     *  -np 1 -mca aaa bbb ./my-app -mca eee fff
     *  $ mpirun -np 2 -mca foo bar --app launch.appfile
     * Only pick up '-mca foo bar' on this pass.
     */
    if (NULL != orun_globals.appfile) {
        if (ORTE_SUCCESS != (rc = capture_cmd_line_params(argc, 0, argv))) {
            goto cleanup;
        }
    }
    
    /* Parse application command line options. */

    init_globals();
    opal_cmd_line_create(&cmd_line, cmd_line_init);
    mca_base_cmd_line_setup(&cmd_line);
    cmd_line_made = true;
    rc = opal_cmd_line_parse(&cmd_line, true, argc, argv);
    if (ORTE_SUCCESS != rc) {
        goto cleanup;
    }
    mca_base_cmd_line_process_args(&cmd_line, app_env, &global_mca_env);

    /* Is there an appfile in here? */

    if (NULL != orun_globals.appfile) {
        OBJ_DESTRUCT(&cmd_line);
        return parse_appfile(jdata, strdup(orun_globals.appfile), app_env);
    }

    /* Setup application context */

    app = OBJ_NEW(orte_app_context_t);
    opal_cmd_line_get_tail(&cmd_line, &count, &app->argv);

    /* See if we have anything left */

    if (0 == count) {
        orte_show_help("help-orun.txt", "orun:executable-not-specified",
                       true, orte_basename, orte_basename);
        rc = ORTE_ERR_NOT_FOUND;
        goto cleanup;
    }

    /*
     * Get mca parameters so we can pass them to the daemons.
     * Use the count determined above to make sure we do not go past
     * the executable name. Example:
     *   orun -np 2 -mca foo bar ./my-app -mca bip bop
     * We want to pick up '-mca foo bar' but not '-mca bip bop'
     */
    if (ORTE_SUCCESS != (rc = capture_cmd_line_params(argc, count, argv))) {
        goto cleanup;
    }
    
    /* Grab all OMPI_* environment variables */

    app->env = opal_argv_copy(*app_env);
    for (i = 0; NULL != environ[i]; ++i) {
        if (0 == strncmp("OMPI_", environ[i], 5)) {
            /* check for duplicate in app->env - this
             * would have been placed there by the
             * cmd line processor. By convention, we
             * always let the cmd line override the
             * environment
             */
            param = strdup(environ[i]);
            value = strchr(param, '=');
            if (value != NULL) {
                *value = '\0';
                value++;
                opal_setenv(param, value, false, &app->env);
            }
            free(param);
        }
    }
    
    /* add the ompi-server, if provided */
    if (NULL != ompi_server) {
        opal_setenv("OMPI_MCA_pubsub_orte_server", ompi_server, true, &app->env);
    }

    /* Did the user request to export any environment variables on the cmd line? */
    if (opal_cmd_line_is_taken(&cmd_line, "x")) {
        j = opal_cmd_line_get_ninsts(&cmd_line, "x");
        for (i = 0; i < j; ++i) {
            param = opal_cmd_line_get_param(&cmd_line, "x", i, 0);

            if (NULL != strchr(param, '=')) {
                opal_argv_append_nosize(&app->env, param);
                /* save it for any comm_spawn'd apps */
                opal_argv_append_nosize(&orte_forwarded_envars, param);
            } else {
                value = getenv(param);
                if (NULL != value) {
                    if (NULL != strchr(value, '=')) {
                        opal_argv_append_nosize(&app->env, value);
                        /* save it for any comm_spawn'd apps */
                        opal_argv_append_nosize(&orte_forwarded_envars, value);
                    } else {
                        asprintf(&value2, "%s=%s", param, value);
                        opal_argv_append_nosize(&app->env, value2);
                        /* save it for any comm_spawn'd apps */
                        opal_argv_append_nosize(&orte_forwarded_envars, value2);
                        free(value2);
                    }
                } else {
                    opal_output(0, "Warning: could not find environment variable \"%s\"\n", param);
                }
            }
        }
    }

    /* Did the user request to export any environment variables via MCA param? */
    if (NULL != orte_forward_envars) {
        char **vars;
        vars = opal_argv_split(orte_forward_envars, ',');
        for (i=0; NULL != vars[i]; i++) {
            if (NULL != strchr(vars[i], '=')) {
                /* user supplied a value */
                opal_argv_append_nosize(&app->env, vars[i]);
                /* save it for any comm_spawn'd apps */
                opal_argv_append_nosize(&orte_forwarded_envars, vars[i]);
            } else {
                /* get the value from the environ */
                value = getenv(vars[i]);
                if (NULL != value) {
                    if (NULL != strchr(value, '=')) {
                        opal_argv_append_nosize(&app->env, value);
                        /* save it for any comm_spawn'd apps */
                        opal_argv_append_nosize(&orte_forwarded_envars, value);
                    } else {
                        asprintf(&value2, "%s=%s", vars[i], value);
                        opal_argv_append_nosize(&app->env, value2);
                        /* save it for any comm_spawn'd apps */
                        opal_argv_append_nosize(&orte_forwarded_envars, value2);
                        free(value2);
                    }
                } else {
                    opal_output(0, "Warning: could not find environment variable \"%s\"\n", vars[i]);
                }
            }
        }
        opal_argv_free(vars);
    }

    /* If the user specified --path, store it in the user's app
       environment via the OMPI_exec_path variable. */
    if (NULL != orun_globals.path) {
        asprintf(&value, "OMPI_exec_path=%s", orun_globals.path);
        opal_argv_append_nosize(&app->env, value);
        /* save it for any comm_spawn'd apps */
        opal_argv_append_nosize(&orte_forwarded_envars, value);
        free(value);
    }

    /* Did the user request a specific wdir? */

    if (NULL != orun_globals.wdir) {
        /* if this is a relative path, convert it to an absolute path */
        if (opal_path_is_absolute(orun_globals.wdir)) {
            app->cwd = strdup(orun_globals.wdir);
        } else {
            /* get the cwd */
            if (OPAL_SUCCESS != (rc = opal_getcwd(cwd, sizeof(cwd)))) {
                orte_show_help("help-orun.txt", "orun:init-failure",
                               true, "get the cwd", rc);
                goto cleanup;
            }
            /* construct the absolute path */
            app->cwd = opal_os_path(false, cwd, orun_globals.wdir, NULL);
        }
        orte_set_attribute(&app->attributes, ORTE_APP_USER_CWD, ORTE_ATTR_GLOBAL, NULL, OPAL_BOOL);
    } else if (orun_globals.set_cwd_to_session_dir) {
        orte_set_attribute(&app->attributes, ORTE_APP_SSNDIR_CWD, ORTE_ATTR_GLOBAL, NULL, OPAL_BOOL);
        orte_set_attribute(&app->attributes, ORTE_APP_USER_CWD, ORTE_ATTR_GLOBAL, NULL, OPAL_BOOL);
    } else {
        if (OPAL_SUCCESS != (rc = opal_getcwd(cwd, sizeof(cwd)))) {
            orte_show_help("help-orun.txt", "orun:init-failure",
                           true, "get the cwd", rc);
            goto cleanup;
        }
        app->cwd = strdup(cwd);
        orte_remove_attribute(&app->attributes, ORTE_APP_USER_CWD);
    }

    /* if this is the first app_context, check for prefix directions.
     * We only do this for the first app_context because the launchers
     * only look at the first one when setting the prefix - we do NOT
     * support per-app_context prefix settings!
     */
    if (0 == total_num_apps) {
        /* Check to see if the user explicitly wanted to disable automatic
           --prefix behavior */
        
        if (opal_cmd_line_is_taken(&cmd_line, "noprefix")) {
            want_prefix_by_default = false;
        }

        /* Did the user specify a prefix, or want prefix by default? */
        if (opal_cmd_line_is_taken(&cmd_line, "prefix") || want_prefix_by_default) {
            size_t param_len;
            /* if both the prefix was given and we have a prefix
             * given above, check to see if they match
             */
            if (opal_cmd_line_is_taken(&cmd_line, "prefix") &&
                NULL != orun_globals.prefix) {
                /* if they don't match, then that merits a warning */
                char *cli_prefix_param;
                cli_prefix_param = opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0);
                if ( NULL != cli_prefix_param ) {
                    param = strdup(cli_prefix_param);
                }
                /* ensure we strip any trailing '/' */
                if (NULL != param && 0 == strcmp(OPAL_PATH_SEP, &(param[strlen(param)-1]))) {
                    param[strlen(param)-1] = '\0';
                }
                value = strdup(orun_globals.prefix);
                if (0 == strcmp(OPAL_PATH_SEP, &(value[strlen(value)-1]))) {
                    value[strlen(value)-1] = '\0';
                }
                if (NULL != param && 0 != strcmp(param, value)) {
                    orte_show_help("help-orun.txt", "orun:app-prefix-conflict",
                                   true, orte_basename, value, param);
                    /* let the global-level prefix take precedence since we
                     * know that one is being used
                     */
                    free(param);
                    param = strdup(orun_globals.prefix);
                }
                free(value);
            } else if (NULL != orun_globals.prefix) {
                param = strdup(orun_globals.prefix);
            } else if (opal_cmd_line_is_taken(&cmd_line, "prefix")){
                /* must be --prefix alone */
                char *cli_prefix_param;
                cli_prefix_param = opal_cmd_line_get_param(&cmd_line, "prefix", 0, 0);
                if ( NULL != cli_prefix_param ) {
                    param = strdup(cli_prefix_param);
                }
            } else {
                /* --enable-orun-prefix-default was given to orun */
                param = strdup(opal_install_dirs.prefix);
            }

            if (NULL != param) {
                /* "Parse" the param, aka remove superfluous path_sep. */
                param_len = strlen(param);
                while (0 == strcmp (OPAL_PATH_SEP, &(param[param_len-1]))) {
                    param[param_len-1] = '\0';
                    param_len--;
                    if (0 == param_len) {
                        orte_show_help("help-orun.txt", "orun:empty-prefix",
                                       true, orte_basename, orte_basename);
                        free(param);
                        return ORTE_ERR_FATAL;
                    }
                }

                orte_set_attribute(&app->attributes, ORTE_APP_PREFIX_DIR, ORTE_ATTR_GLOBAL, (void**)&param, OPAL_STRING);
                free(param);
            }
        }
    }

    /* Did the user specify a hostfile. Need to check for both 
     * hostfile and machine file. 
     * We can only deal with one hostfile per app context, otherwise give an error.
     */
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "hostfile"))) {
        if(1 < j) {
            orte_show_help("help-orun.txt", "orun:multiple-hostfiles",
                           true, orte_basename, NULL);
            return ORTE_ERR_FATAL;
        } else {
            value = opal_cmd_line_get_param(&cmd_line, "hostfile", 0, 0);
            orte_set_attribute(&app->attributes, ORTE_APP_HOSTFILE, ORTE_ATTR_GLOBAL, (void**)&value, OPAL_STRING);
        }
    }
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "machinefile"))) {
        if(1 < j || orte_get_attribute(&app->attributes, ORTE_APP_HOSTFILE, NULL, OPAL_STRING)) {
            orte_show_help("help-orun.txt", "orun:multiple-hostfiles",
                           true, orte_basename, NULL);
            return ORTE_ERR_FATAL;
        } else {
            value = opal_cmd_line_get_param(&cmd_line, "machinefile", 0, 0);
            orte_set_attribute(&app->attributes, ORTE_APP_HOSTFILE, ORTE_ATTR_GLOBAL, (void**)&value, OPAL_STRING);
        }
    }
 
    /* Did the user specify any hosts? */
    if (0 < (j = opal_cmd_line_get_ninsts(&cmd_line, "host"))) {
        char **dh=NULL, *dptr;
        for (i = 0; i < j; ++i) {
            value = opal_cmd_line_get_param(&cmd_line, "host", i, 0);
            opal_argv_append_nosize(&dh, value);
        }
        dptr = opal_argv_join(dh, ',');
        opal_argv_free(dh);
        orte_set_attribute(&app->attributes, ORTE_APP_DASH_HOST, ORTE_ATTR_GLOBAL, (void**)&dptr, OPAL_STRING);
        free(dptr);
    }

    /* check for bozo error */
    if (0 > orun_globals.num_procs) {
        orte_show_help("help-orun.txt", "orun:negative-nprocs",
                       true, orte_basename, app->argv[0],
                       orun_globals.num_procs, NULL);
        return ORTE_ERR_FATAL;
    }

    app->num_procs = (orte_std_cntr_t)orun_globals.num_procs;
    total_num_apps++;

    /* Capture any preload flags */
    if (orun_globals.preload_binaries) {
        orte_set_attribute(&app->attributes, ORTE_APP_PRELOAD_BIN, ORTE_ATTR_GLOBAL, NULL, OPAL_BOOL);
    }
    /* if we were told to cwd to the session dir and the app was given in
     * relative syntax, then we need to preload the binary to
     * find the app - don't do this for java apps, however, as we
     * can't easily find the class on the cmd line. Java apps have to
     * preload their binary via the preload_files option
     */
    if (orte_get_attribute(&app->attributes, ORTE_APP_SSNDIR_CWD, NULL, OPAL_BOOL) &&
        !opal_path_is_absolute(app->argv[0]) &&
        NULL == strstr(app->argv[0], "java")) {
        orte_set_attribute(&app->attributes, ORTE_APP_PRELOAD_BIN, ORTE_ATTR_GLOBAL, NULL, OPAL_BOOL);
    }
    if (NULL != orun_globals.preload_files) {
        orte_set_attribute(&app->attributes, ORTE_APP_PRELOAD_FILES, ORTE_ATTR_GLOBAL,
                           (void*)orun_globals.preload_files, OPAL_STRING);
    }

#if OPAL_ENABLE_FT_CR == 1
    if( NULL != orun_globals.sstore_load ) {
        app->sstore_load = strdup(orun_globals.sstore_load);
    } else {
        app->sstore_load = NULL;
    }
#endif

    /* Do not try to find argv[0] here -- the starter is responsible
       for that because it may not be relevant to try to find it on
       the node where orun is executing.  So just strdup() argv[0]
       into app. */

    app->app = strdup(app->argv[0]);
    if (NULL == app->app) {
        orte_show_help("help-orun.txt", "orun:call-failed",
                       true, orte_basename, "library", "strdup returned NULL", errno);
        rc = ORTE_ERR_NOT_FOUND;
        goto cleanup;
    }

    /* if this is a Java application, we have a bit more work to do. Such
     * applications actually need to be run under the Java virtual machine
     * and the "java" command will start the "executable". So we need to ensure
     * that all the proper java-specific paths are provided
     */
    appname = opal_basename(app->app);
    if (0 == strcmp(appname, "java")) {
        /* see if we were given a library path */
        found = false;
        for (i=1; NULL != app->argv[i]; i++) {
            if (NULL != strstr(app->argv[i], "java.library.path")) {
                /* yep - but does it include the path to the mpi libs? */
                found = true;
                if (NULL == strstr(app->argv[i], opal_install_dirs.libdir)) {
                    /* doesn't appear to - add it to be safe */
                    if (':' == app->argv[i][strlen(app->argv[i]-1)]) {
                        asprintf(&value, "-Djava.library.path=%s%s", app->argv[i], opal_install_dirs.libdir);
                    } else {
                        asprintf(&value, "-Djava.library.path=%s:%s", app->argv[i], opal_install_dirs.libdir);
                    }
                    free(app->argv[i]);
                    app->argv[i] = value;
                }
                break;
            }
        }
        if (!found) {
            /* need to add it right after the java command */
            asprintf(&value, "-Djava.library.path=%s", opal_install_dirs.libdir);
            opal_argv_insert_element(&app->argv, 1, value);
            free(value);
        }
        
        /* see if we were given a class path */
        found = false;
        for (i=1; NULL != app->argv[i]; i++) {
            if (NULL != strstr(app->argv[i], "cp") ||
                NULL != strstr(app->argv[i], "classpath")) {
                /* yep - but does it include the path to the mpi libs? */
                found = true;
                if (NULL == strstr(app->argv[i+1], "mpi.jar")) {
                    /* nope - need to add it */
                    if (':' == app->argv[i+1][strlen(app->argv[i+1]-1)]) {
                        asprintf(&value, "%s%s/mpi.jar", app->argv[i+1], opal_install_dirs.libdir);
                    } else {
                        asprintf(&value, "%s:%s/mpi.jar", app->argv[i+1], opal_install_dirs.libdir);
                    }
                    free(app->argv[i+1]);
                    app->argv[i+1] = value;
                }
                break;
            }
        }
        if (!found) {
            /* check to see if CLASSPATH is in the environment */
            for (i=0; NULL != environ[i]; i++) {
                if (0 == strncmp(environ[i], "CLASSPATH", strlen("CLASSPATH"))) {
                    /* check if mpi.jar is present */
                    if (NULL != strstr(environ[i], "mpi.jar")) {
                        /* yes - just add the envar to the argv in the
                         * right format
                         */
                        value = strchr(environ[i], '=');
                        ++value; /* step over the = */
                        opal_argv_insert_element(&app->argv, 1, value);
                        opal_argv_insert_element(&app->argv, 1, "-cp");
                    } else {
                        /* need to add it */
                        value = strchr(environ[i], '=');
                        ++value; /* step over the = */
                        if (':' == value[strlen(value-1)]) {
                            asprintf(&param, "%s%s/mpi.jar", value, opal_install_dirs.libdir);
                        } else {
                            asprintf(&param, "%s:%s/mpi.jar", value, opal_install_dirs.libdir);
                        }
                        opal_argv_insert_element(&app->argv, 1, param);
                        opal_argv_insert_element(&app->argv, 1, "-cp");
                        free(param);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                /* need to add it right after the java command - have
                 * to include the current directory and trust that
                 * the user set cwd if necessary
                 */
                asprintf(&value, ".:%s/mpi.jar", opal_install_dirs.libdir);
                opal_argv_insert_element(&app->argv, 1, value);
                free(value);
                opal_argv_insert_element(&app->argv, 1, "-cp");
            }
        }
        /* try to find the actual command - may not be perfect */
        for (i=1; i < opal_argv_count(app->argv); i++) {
            if (NULL != strstr(app->argv[i], "java.library.path")) {
                continue;
            } else if (NULL != strstr(app->argv[i], "cp") ||
                       NULL != strstr(app->argv[i], "classpath")) {
                /* skip the next field */
                i++;
                continue;
            }
            /* declare this the winner */
            opal_setenv("OMPI_COMMAND", app->argv[i], true, &app->env);
            /* collect everything else as the cmd line */
            if ((i+1) < opal_argv_count(app->argv)) {
                value = opal_argv_join(&app->argv[i+1], ' ');
                opal_setenv("OMPI_ARGV", value, true, &app->env);
                free(value);
            }
            break;
        }
    } else {
        /* add the cmd to the environment for MPI_Info to pickup */
        opal_setenv("OMPI_COMMAND", appname, true, &app->env);
        if (1 < opal_argv_count(app->argv)) {
            value = opal_argv_join(&app->argv[1], ' ');
            opal_setenv("OMPI_ARGV", value, true, &app->env);
            free(value);
        }
    }
    free(appname);
    
    *app_ptr = app;
    app = NULL;
    *made_app = true;

    /* All done */

 cleanup:
    if (NULL != app) {
        OBJ_RELEASE(app);
    }
    if (cmd_line_made) {
        OBJ_DESTRUCT(&cmd_line);
    }
    return rc;
}


static int parse_appfile(orte_job_t *jdata, char *filename, char ***env)
{
    size_t i, len;
    FILE *fp;
    char line[BUFSIZ];
    int rc, argc, app_num;
    char **argv;
    orte_app_context_t *app;
    bool blank, made_app;
    char bogus[] = "bogus ";
    char **tmp_env;

    /*
     * Make sure to clear out this variable so we don't do anything odd in
     * app_create()
     */
    if( NULL != orun_globals.appfile ) {
        free( orun_globals.appfile );
        orun_globals.appfile =     NULL;
    }

    /* Try to open the file */

    fp = fopen(filename, "r");
    if (NULL == fp) {
        orte_show_help("help-orun.txt", "orun:appfile-not-found", true,
                       filename);
        return ORTE_ERR_NOT_FOUND;
    }

    /* Read in line by line */

    line[sizeof(line) - 1] = '\0';
    app_num = 0;
    do {

        /* We need a bogus argv[0] (because when argv comes in from
           the command line, argv[0] is "orun", so the parsing
           logic ignores it).  So create one here rather than making
           an argv and then pre-pending a new argv[0] (which would be
           rather inefficient). */

        line[0] = '\0';
        strcat(line, bogus);

        if (NULL == fgets(line + sizeof(bogus) - 1,
                          sizeof(line) - sizeof(bogus) - 1, fp)) {
            break;
        }

        /* Remove a trailing newline */

        len = strlen(line);
        if (len > 0 && '\n' == line[len - 1]) {
            line[len - 1] = '\0';
            if (len > 0) {
                --len;
            }
        }

        /* Remove comments */

        for (i = 0; i < len; ++i) {
            if ('#' == line[i]) {
                line[i] = '\0';
                break;
            } else if (i + 1 < len && '/' == line[i] && '/' == line[i + 1]) {
                line[i] = '\0';
                break;
            }
        }

        /* Is this a blank line? */

        len = strlen(line);
        for (blank = true, i = sizeof(bogus); i < len; ++i) {
            if (!isspace(line[i])) {
                blank = false;
                break;
            }
        }
        if (blank) {
            continue;
        }

        /* We got a line with *something* on it.  So process it */

        argv = opal_argv_split(line, ' ');
        argc = opal_argv_count(argv);
        if (argc > 0) {

            /* Create a temporary env to use in the recursive call --
               that is: don't disturb the original env so that we can
               have a consistent global env.  This allows for the
               case:

                   orun --mca foo bar --appfile file

               where the "file" contains multiple apps.  In this case,
               each app in "file" will get *only* foo=bar as the base
               environment from which its specific environment is
               constructed. */

            if (NULL != *env) {
                tmp_env = opal_argv_copy(*env);
                if (NULL == tmp_env) {
                    fclose(fp);
                    return ORTE_ERR_OUT_OF_RESOURCE;
                }
            } else {
                tmp_env = NULL;
            }

            rc = create_app(argc, argv, jdata, &app, &made_app, &tmp_env);
            if (ORTE_SUCCESS != rc) {
                /* Assume that the error message has already been
                   printed; no need to cleanup -- we can just exit */
                fclose(fp);
                exit(1);
            }
            if (NULL != tmp_env) {
                opal_argv_free(tmp_env);
            }
            if (made_app) {
                app->idx = app_num;
                ++app_num;
                opal_pointer_array_add(jdata->apps, app);
                ++jdata->num_apps;
            }
        }
    } while (!feof(fp));
    fclose(fp);

    /* All done */

    free(filename);
    return ORTE_SUCCESS;
}

/* scheduler specific arguments */
static int init_sched_args(void)
{
    /* Only CONSTRUCT things once */
    orun_globals.alloc_request = false;     /* new allocation */
    orun_globals.account = '\0';     /* account */
    orun_globals.name = '\0';     /* name */
    orun_globals.gid =   -1;       /* gid */
    orun_globals.max_nodes = 0;        /* max_nodes */
    orun_globals.max_pes = 0;        /* max_pes */
    orun_globals.min_nodes = 0;        /* min_nodes */
    orun_globals.min_pes = 0;        /* min_pes */
    orun_globals.starttime = '\0';     /* starttime */
    orun_globals.walltime =  '\0';     /* walltime */
    orun_globals.exclusive = false;    /* exclusive */
    orun_globals.interactive = true;    /* interactive */
    orun_globals.nodefile = '\0';     /* nodefile */
    orun_globals.resource = '\0';    /* resources */

    return ORTE_SUCCESS;
}


static int parse_args_sched(int argc, char *argv[], opal_cmd_line_t *cmd_line)
{
    char *str = NULL;
    /**
     * Now start parsing scheduler specific arguments
     */

    /* if user hasn't supplied a group to run under, use effective gid of caller */
    /* TODO: double check if user is in group */
    /* do we also need to support the name as well as id? */

    if (orun_globals.max_nodes || orun_globals.min_nodes) {
       orun_globals.alloc_request = true;
    } 

    if (-1 == orun_globals.gid) {
        asprintf(&str, "%u", getgid());
        orun_globals.gid = (int)strtol(str, NULL, 10);
        free(str);
    }

    if (orun_globals.max_nodes < orun_globals.min_nodes) {
       orun_globals.max_nodes = orun_globals.min_nodes;
    } 
    if (orun_globals.max_pes < orun_globals.min_pes) {
       orun_globals.max_pes = orun_globals.min_pes;
    } 

    return ORTE_SUCCESS;
}

static int alloc_request( orcm_alloc_t *alloc )
{
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, n;
    orcm_scd_cmd_flag_t command=ORCM_SESSION_REQ_COMMAND;
    orcm_alloc_id_t id;
    struct timeval tv;

    alloc->id = 0;                                // session priority
    alloc->priority = 1;                                // session priority
    alloc->account = orun_globals.account;         // account to be charged
    alloc->name = orun_globals.name;               // user-assigned project name
    alloc->gid = orun_globals.gid;                 // group id to be run under
    alloc->max_nodes = orun_globals.max_nodes;     // max number of nodes
    alloc->max_pes = orun_globals.max_pes;         // max number of processing elements
    alloc->min_nodes = orun_globals.min_nodes;     // min number of nodes required
    alloc->min_pes = orun_globals.min_pes;         // min number of pe's required
    alloc->exclusive = orun_globals.exclusive;     // true if nodes to be exclusively allocated (i.e., not shared across sessions)
    alloc->interactive = orun_globals.interactive; // true if in interactive mode
    alloc->nodes = '\0';                                // regex of nodes to be used
    /* alloc->constraints = orun_globals.resources */ ; // list of resource constraints to be applied when selecting hosts

    alloc->caller_uid = getuid();   // caller uid, not from args
    alloc->caller_gid = getgid();   // caller gid, not from args
    alloc->hnpname = '\0';
    alloc->hnpuri = '\0';
    alloc->parent_name = ORTE_NAME_PRINT(ORTE_PROC_MY_NAME);
    alloc->parent_uri = orte_rml.get_contact_info();

    if (NULL == orun_globals.starttime || 0 == strlen(orun_globals.starttime)) {
        gettimeofday(&tv,NULL);
        /* desired start time for allocation deafults to now */
        alloc->begin = tv.tv_sec;
    } else {
        /* TODO: eventually parse the string to figure out what user means, for now its now */
        gettimeofday(&tv,NULL);
        alloc->begin = tv.tv_sec;
    }

    if (NULL == orun_globals.walltime || 0 == strlen(orun_globals.walltime)) {
        /* desired walltime default to 10 min */
        alloc->walltime = 600;
    } else {
        /* get this in seconds for now, but will be parsed for more complexity later */
        alloc->walltime = (time_t)strtol(orun_globals.walltime, NULL, 10);                               // max execution time
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &alloc, 1, ORCM_ALLOC))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* get our allocated jobid */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &id, &n, ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    alloc->id = id;

    return ORTE_SUCCESS;
}
