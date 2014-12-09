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


static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
}

static int ethtest_log(opal_buffer_t *buf)
{
    time_t diag_time;
    int cnt, rc;
    char *nodename;
    opal_list_t *vals;
    opal_value_t *kv;

    /* Unpack the Time */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buf, &diag_time,
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

    /* Unpack our results */

    /* create opal value array to send to db */
    vals = OBJ_NEW(opal_list_t);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("time");
    kv->type = OPAL_TIMEVAL;
    kv->data.tv.tv_sec = diag_time;
    opal_list_append(vals, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("nodename");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(nodename);
    opal_list_append(vals, &kv->super);
    free(nodename);

    /* add results */

    /* send to db */
    if (0 <= orcm_diag_base.dbhandle) {
        /* TODO: change this to whatever db function will be used for diags */
        orcm_db.store(orcm_diag_base.dbhandle, "ethtest", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    return ORCM_SUCCESS;
}

static void ethtest_run(int sd, short args, void *cbdata)
{
    orcm_diag_caddy_t *caddy = (orcm_diag_caddy_t*)cbdata;
    int eth_diag_ret = 0;
    int sock;
    struct ifreq ifr;
    struct ethtool_test *eth_test;
    struct ethtool_gstrings *gstring;
    struct ethtool_drvinfo drvinfo;
    int testinfo_offset;
    int i, ret, rc;
    int strset_len;
    char *resource = strdup("eth0");
    orcm_diag_cmd_flag_t command = ORCM_DIAG_AGG_COMMAND;
    opal_buffer_t *data = NULL;
    time_t now;
    char *compname;
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
        free(gstring);
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
        free(gstring);
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
        free(eth_test);
        free(gstring);
        goto sendresults;
    } else {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag: %s - ethtool ioctl TEST completed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), resource );
    }

    ret = dump_eth_test(resource, eth_test, gstring);
    free(eth_test);
    free(gstring);
    free(resource);

sendresults:
    now = time(NULL);
    data = OBJ_NEW(opal_buffer_t);

    /* pack aggregator command */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &command, 1, ORCM_DIAG_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* pack our component name */
    compname = strdup("ethtest");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &compname, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(compname);

    /* Pack the Time */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &now, 1, OPAL_TIME))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack our node name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Pack our results */

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
    opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                        "%s diag:Ethernet test result : %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ret ? "FAIL" : "PASS");

    if ( strings->len ) {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                        "%s diag:Ethernet test details",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME) );

    }

    for (i = 0; i < strings->len; i++) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                        "%s diag: %8s   %15s     %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        resource,
                        (char *)(strings->data + i * ETH_GSTRING_LEN),
                        (long)eth_test->data[i] ? "[ FAIL ]" : "[  OK  ]" );
    }

    return ret;
}

