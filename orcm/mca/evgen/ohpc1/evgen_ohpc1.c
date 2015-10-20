/*
 * Copyright (c) 2014-2015 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/argv.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"

#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orcm/mca/evgen/base/base.h"
#include "orcm/mca/evgen/ohpc1/evgen_ohpc1.h"

/* API functions */

static void ohpc1_init(void);
static void ohpc1_finalize(void);
static void generate(orcm_ras_event_t *cd);

/* The module struct */

orcm_evgen_base_module_t orcm_evgen_ohpc1_module = {
    ohpc1_init,
    ohpc1_finalize,
    generate
};

typedef struct {
    opal_object_t super;
    bool active;
    int status;
    int dbhandle;
    opal_list_t *list;
} ohpc1_caddy_t;
static void cdcon(ohpc1_caddy_t *p)
{
    p->list = NULL;
    p->active = true;
}
static void cddes(ohpc1_caddy_t *p)
{
    if (NULL != p->list) {
        OPAL_LIST_RELEASE(p->list);
    }
}
static OBJ_CLASS_INSTANCE(ohpc1_caddy_t,
                          opal_object_t,
                          cdcon, cddes);

/* local variables */
static bool got_handle = false;
static int dbhandle = -1;

static void cbfunc(int dbh, int status,
                   opal_list_t *in, opal_list_t *out,
                   void *cbdata)
{
    ohpc1_caddy_t *cd = (ohpc1_caddy_t*)cbdata;
    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:ohpc1 cbfunc for handle %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), dbh));

    cd->status = status;
    cd->dbhandle = dbh;
    cd->active = false;
}

static void ohpc1_init(void)
{
    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:ohpc1 init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    return;
}

static void ohpc1_finalize(void)
{
    ohpc1_caddy_t *cd;

    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:ohpc1 finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (0 <= dbhandle) {
        /* setup a caddy */
        cd = OBJ_NEW(ohpc1_caddy_t);
        orcm_db.close(dbhandle, cbfunc, cd);
        ORTE_WAIT_FOR_COMPLETION(cd->active);
        dbhandle = -1;
        OBJ_RELEASE(cd);
    }
    return;
}

static void generate(orcm_ras_event_t *ecd)
{
    ohpc1_caddy_t *cd;
    opal_value_t *kv;

    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:ohpc1 record event",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (!got_handle) {
        /* get a db handle */
        cd = OBJ_NEW(ohpc1_caddy_t);

        /* get a dbhandle for us */
        cd->list = OBJ_NEW(opal_list_t);
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("components");
        kv->type = OPAL_STRING;
        kv->data.string = strdup("print");
        opal_list_append(cd->list, &kv->super);
        orcm_db.open("ohpc1", cd->list, cbfunc, cd);
        ORTE_WAIT_FOR_COMPLETION(cd->active);
        if (ORCM_SUCCESS == cd->status && 0 <= cd->dbhandle) {
            dbhandle = cd->dbhandle;
            got_handle = true;
        }
        OBJ_RELEASE(cd);
    }
    cd = OBJ_NEW(ohpc1_caddy_t);
    cd->list = OBJ_NEW(opal_list_t);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("TYPE");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(orcm_evgen_base_print_type(ecd->type));
    opal_list_append(cd->list, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("SEVERITY");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(orcm_evgen_base_print_severity(ecd->severity));
    opal_list_append(cd->list, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("TIME");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(ctime(&ecd->timestamp));
    opal_list_append(cd->list, &kv->super);

    orcm_db.store(dbhandle, "RAS-EVENT", cd->list, cbfunc, cd);
    ORTE_WAIT_FOR_COMPLETION(cd->active);
    OBJ_RELEASE(cd);

    return;
}
