/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>

#include <sys/mman.h>
#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/ethtool.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "opal/class/opal_list.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"
#include "opal/util/os_path.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/hwloc/hwloc.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/diag/base/base.h"
#include "diag_ethtest.h"

static int init(void);
static void finalize(void);
static int ethtest_log(opal_buffer_t *buf);
static void ethtest_run(int sd, short args, void *cbdata);

orcm_diag_base_module_t orcm_diag_ethtest_module = {
    init,
    finalize,
    NULL,
    ethtest_log,
    ethtest_run
};

static int dump_eth_test(char *resource,
                         struct ethtool_test *eth_test,
                         struct ethtool_gstrings *strings);


static int init(void)
{
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                         "%s diag:ethtest:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    return ORCM_SUCCESS;

}

static void finalize(void)
{
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                         "%s diag:ethtest:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );
    return;
}


static int ethtest_log(opal_buffer_t *buf)
{
    int cnt, rc;
    time_t start_time;
    time_t end_time;
    struct tm *starttime;
    struct tm *endtime;
    char *nodename;
    char *diag_subtype;
    char *diag_result;
    int result_num = 0;
    int subtest_result = 0;

    /* Unpack start Time */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &start_time,
                                              &cnt, OPAL_TIME))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack end time */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &end_time,
                                              &cnt, OPAL_TIME))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack the node name */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &nodename,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack overall results */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_result,
                                              &cnt, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* Unpack number of result details */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &result_num,
                                              &cnt, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    starttime = localtime(&start_time);
    endtime   = localtime(&end_time);

    /* send overall diag test result to db */
    if (0 <= orcm_diag_base.dbhandle) {
        orcm_db.record_diag_test(orcm_diag_base.dbhandle, nodename, "ethernet", "all", starttime, endtime,
                                 NULL, diag_result, NULL, NULL, NULL);
    }

    /* add results in details */
    while ( result_num ) {
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_subtype,
                                              &cnt, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }

        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &subtest_result,
                                              &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }

        /* send detail diag test result to db */
        if (0 <= orcm_diag_base.dbhandle) {
            orcm_db.record_diag_test(orcm_diag_base.dbhandle, nodename, "ethernet", diag_subtype, starttime, endtime, 
                                 NULL, subtest_result ? "FAIL" : "PASS", NULL, NULL, NULL);
        }
        result_num--;
    }


    return ORCM_SUCCESS;
}

static void ethtest_run(int sd, short args, void *cbdata)
{
    orcm_diag_caddy_t *caddy = (orcm_diag_caddy_t*)cbdata;
    int eth_diag_ret = 0;
    int sock;
    struct ifreq ifr;
    struct ethtool_test *eth_test = NULL;
    struct ethtool_gstrings *gstring = NULL;
    struct ethtool_drvinfo drvinfo;
    int testinfo_offset;
    int i, ret, rc;
    int strset_len;
    char *resource = strdup("eth0");
    orcm_diag_cmd_flag_t command = ORCM_DIAG_AGG_COMMAND;
    opal_buffer_t *data = NULL;
    time_t now;
    time_t start_time;
    char *compname;
    char *diag_result;
    char *diag_subtype;
    int  result_num = 0;
    orte_process_name_t *tgt;

    if (ORTE_PROC_IS_CM) {
        /* we send to our daemon */
        tgt = ORTE_PROC_MY_DAEMON;
    } else {
        tgt = ORTE_PROC_MY_HNP;
    }
    /* if my target hasn't been defined yet, ignore - nobody listening yet */
    if (ORTE_JOBID_INVALID ==tgt->jobid ||
        ORTE_VPID_INVALID == tgt->vpid) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:ethtest: HNP is not defined",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* still run diags? */
        /* return; */
    }

    start_time = time(NULL);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:cannot open socket",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        goto sendresults;
    }

    /* TODO: pass interface(s) via options list */
    strncpy(ifr.ifr_name, resource, sizeof(ifr.ifr_name));

     /* Get string set length */
    testinfo_offset = (int)offsetof(struct ethtool_drvinfo, testinfo_len);

    /* Get driver info for old kernel version */
    /* New kernel uses ETHTOOL_GSSET_INFO */
    drvinfo.cmd  = ETHTOOL_GDRVINFO;
    ifr.ifr_data = &drvinfo;

    ret = ioctl(sock, SIOCETHTOOL, &ifr);
    if ( ret ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:ethtool ioctl GDRVINFO failed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        goto sendresults;
    }
    strset_len = *(int *)((char *)&drvinfo + testinfo_offset);

    gstring = calloc(1, sizeof(*gstring) + strset_len * ETH_GSTRING_LEN);
    if ( !gstring ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:calloc failed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        goto sendresults;
    }

    /* Get specified string set for test info */
    gstring->cmd        = ETHTOOL_GSTRINGS;
    gstring->string_set = ETH_SS_TEST;
    gstring->len        = strset_len;
    ifr.ifr_data        = gstring;

    ret = ioctl(sock, SIOCETHTOOL, &ifr);
    if ( ret ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:ethtool ioctl GSTRINGS failed - %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        goto sendresults;
    }

    /* Terminate with NULL */
    for (i = 0; i < strset_len; i++) {
        gstring->data[(i + 1)*ETH_GSTRING_LEN - 1] = 0;
    }

    eth_test = calloc(1, sizeof(*eth_test) + gstring->len * sizeof(long long));
    if ( !eth_test ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:calloc failed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        goto sendresults;
    }

    /* Initiate device self test */
    memset(eth_test->data, 0, gstring->len * sizeof(long long));
    eth_test->cmd   = ETHTOOL_TEST;
    eth_test->len   = gstring->len;
    /* Test flag set to ONLINE */
    eth_test->flags = 0;

    ifr.ifr_data = eth_test;

    ret = ioctl(sock, SIOCETHTOOL, &ifr);
    if ( ret < 0 ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:ethtool ioctl TEST failed - %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), strerror(errno));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        result_num = 0;
        goto sendresults;
    } else {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag: %s - ethtool ioctl TEST completed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), resource );
    }

    result_num = gstring->len;
    ret = dump_eth_test(resource, eth_test, gstring);

sendresults:
    free(resource);
    close(sock);
    if ( eth_diag_ret & DIAG_ETH_NOTRUN ) {
        diag_result = strdup("NOTRUN");
    } else if ( eth_test->flags & ETH_TEST_FL_FAILED ) {
        diag_result = strdup("FAIL");
        result_num = 0;
    } else {
        diag_result = strdup("PASS");
    }

    now = time(NULL);
    data = OBJ_NEW(opal_buffer_t);

    /* pack aggregator command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &command, 1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }

    /* pack our component name */
    compname = strdup("ethtest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &compname, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }
    free(compname);

    /* Pack start Time */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &start_time, 1, OPAL_TIME))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }

    /* Pack the Time */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &now, 1, OPAL_TIME))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }

    /* Pack our node name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }

    /* Pack overall result */

    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &diag_result, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        free(diag_result);
        return;
    }
    free(diag_result);

    /* Pack the number of result details */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &result_num, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        if ( NULL != eth_test ) {
            free(eth_test);
        }
        if ( NULL != gstring ) {
            free(gstring);
        }
        return;
    }

    /* Pack result details */
    for (i = 0; i < result_num; i++) {
        diag_subtype = strdup((char *)(gstring->data + i * ETH_GSTRING_LEN));
        if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &diag_subtype, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            if ( NULL != eth_test ) {
                free(eth_test);
            }
            if ( NULL != gstring ) {
                free(gstring);
            }
            return;
        }
        free(diag_subtype);

        if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &(eth_test->data[i]), 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            if ( NULL != eth_test ) {
                free(eth_test);
            }
            if ( NULL != gstring ) {
                free(gstring);
            }
            return;
        }
    }

    free(eth_test);
    free(gstring);
    /* send results to aggregator/sender */
    if (ORCM_SUCCESS != (rc = orte_rml.send_buffer_nb(tgt, data,
                                                      ORCM_RML_TAG_DIAG,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(data);
    }

    OBJ_RELEASE(caddy);
    return;
}

static int dump_eth_test(char *resource,
                         struct ethtool_test *eth_test,
                         struct ethtool_gstrings *strings)
{
    unsigned int i   = 0;
    int ret = 0;

    ret = eth_test->flags & ETH_TEST_FL_FAILED;
    opal_output(0, "%s Diagnostic checking ethernet:          \t\t%s\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ret ? "[ FAIL ]" : "[ PASS ]");

    if ( strings->len ) {
        opal_output(0, "%s Diagnostic ethernet test details:\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );

    }

    for (i = 0; i < strings->len; i++) {
        opal_output(0, "%s Diagnostic checking: %8s %15s   %s\n",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        resource,
                        (char *)(strings->data + i * ETH_GSTRING_LEN),
                        (long)eth_test->data[i] ? "[ FAIL ]" : "[ PASS ]" );
    }

    return ret;
}

