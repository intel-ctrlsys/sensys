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

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/sensor/base/sensor_private.h"

#include "orcm/mca/diag/base/base.h"
#include "diag_ethtest.h"

static int init(void);
static void finalize(void);
static int diag_read(opal_list_t *config);
static int diag_check(char *resource, opal_list_t *config);


orcm_diag_base_module_t orcm_diag_ethtest_module = {
    init,
    finalize,
    NULL,
    diag_read,
    diag_check
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


static int diag_read(opal_list_t *config)
{
    return ORCM_SUCCESS;
}

static int diag_check(char *resource, opal_list_t *config)
{
    int eth_diag_ret = 0;
    int sock;
    struct ifreq ifr;
    struct ethtool_test *eth_test;
    struct ethtool_gstrings *gstring;
    struct ethtool_drvinfo drvinfo;
    int testinfo_offset;
    int i, ret;
    int strset_len;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:cannot open socket",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        return eth_diag_ret;
    }

    strncpy(ifr.ifr_name, resource, sizeof(ifr.ifr_name));

     /* Get string set length */
    testinfo_offset          = (int)offsetof(struct ethtool_drvinfo, testinfo_len);

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
        return eth_diag_ret;
    }
    strset_len = *(int *)((char *)&drvinfo + testinfo_offset);

    gstring = calloc(1, sizeof(*gstring) + strset_len * ETH_GSTRING_LEN);
    if ( !gstring ) {
        opal_output_verbose(1, orcm_diag_base_framework.framework_output,
                            "%s diag:calloc failed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        eth_diag_ret |= DIAG_ETH_NOTRUN;
        return eth_diag_ret;
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
        return eth_diag_ret;
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
        return eth_diag_ret;
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
        return eth_diag_ret;
    } else {
        opal_output_verbose(5, orcm_diag_base_framework.framework_output,
                            "%s diag: %s - ethtool ioctl TEST completed",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), resource );
    }

    ret = dump_eth_test(resource, eth_test, gstring);
    free(eth_test);
    free(gstring);

    return eth_diag_ret;
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

