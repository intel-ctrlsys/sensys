/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * @file:
 *
 */

#ifndef MCA_EVGEN_TYPES_H
#define MCA_EVGEN_TYPES_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/types.h"

#include <time.h>

#include "orcm/mca/mca.h"

BEGIN_C_DECLS

/* define RAS severity levels - note that the individual
 * implementations can compress these as desired. However,
 * other subsystems are free to report using any of the
 * defined abstraction values.
 *
 * The values used here are loosely modeled on the standard
 * syslog severity definitions.
 *
 * Note that these are the severities that will be
 * supplied by public ORCM components - vendors and users
 * that create their own components may extend these values
 * at will. Thus, evgen plugins must be prepared to deal
 * with unrecognized severity levels */
#define ORCM_RAS_SEVERITY_EMERG       0
#define ORCM_RAS_SEVERITY_FATAL       1
#define ORCM_RAS_SEVERITY_ALERT       2
#define ORCM_RAS_SEVERITY_CRIT        3
#define ORCM_RAS_SEVERITY_ERROR       4
#define ORCM_RAS_SEVERITY_WARNING     5
#define ORCM_RAS_SEVERITY_NOTICE      6
#define ORCM_RAS_SEVERITY_INFO        7
#define ORCM_RAS_SEVERITY_TRACE       8
#define ORCM_RAS_SEVERITY_DEBUG       9
#define ORCM_RAS_SEVERITY_UNKNOWN    10


/* define the RAS event types. Note that these are the
 * event types that will be supplied by public ORCM
 * components - vendors and users that create their own
 * components may extend these values  at will. Thus, evgen
 * plugins must be prepared to deal with unrecognized
 * event types */
#define ORCM_RAS_EVENT_SENSOR               0
#define ORCM_RAS_EVENT_EXCEPTION            1
#define ORCM_RAS_EVENT_COUNTER              2
#define ORCM_RAS_EVENT_STATE_TRANSITION     3
#define ORCM_RAS_EVENT_UNKNOWN_TYPE         4

/* define a callback function that will be executed
 * once the event generator has finished with the
 * provided orcm_ras_event_t */
typedef void (*orcm_ras_evgen_cbfunc_t)(void *cbdata);

/* define a RAS event structure - note that this is an
 * abstracted form that is intended to support multiple
 * implementations as the exact definition of events may
 * change over versions and for specific systems. Thus,
 * the intent here is to provide some generalized fields
 * that each plugin can implement/extend as required.
 * RAS events are received in this generalized form - the
 * active plugins convert this to their specific format for
 * recording and reporting */
typedef struct {
    opal_object_t super;

    /* provide an event so this can be posted to the
     * evgen progress thread */
    opal_event_t ev;

    /* provide a list of opal_value_t that describe
     * the physical entity in the system that generated
     * this RAS event. This should include at least the
     * type of device that generated the event (e.g.,
     * "cooling dist unit" with a value of "flow controller")
     * and its physical location (e.g., "row":a, rack":3,
     * "blade":0). Each plugin is allowed to compress/truncate
     * the reporter's identification as required to fit
     * their format */
    opal_list_t reporter;

    /* identify the type of RAS event */
    int type;

    /* the time this event was sensed - note that this
     * will be expressed as a local time, and that clocks
     * may well differ across the cluster */
    time_t timestamp;

    /* the severity of the event */
    int severity;

    /* allow passing of any descriptive information
     * about the event itself that specific implementations
     * might find useful in further categorizing the
     * event - passed as a list of opal_value_t. Each
     * plugin is allowed to use or ignore this information.
     * At a minimum, the reporter should include something
     * explaining the error: e.g., "flow low", or
     * "temp high", along with the value that caused it */
    opal_list_t description;

    /* allow passing of any associated data as a list
     * of opal_value_t objects. This might include the
     * values of other sensors at the time of the event,
     * or for a window of sensor values around that time */
    opal_list_t data;

    /* the callback function, if once was given */
    orcm_ras_evgen_cbfunc_t cbfunc;
    void *cbdata;

} orcm_ras_event_t;
OBJ_CLASS_DECLARATION(orcm_ras_event_t);

/* define a collection of attributes to serve as keys for
 * list values. Note that these are the keys that will be
 * supplied by public ORCM components - vendors and users
 * that create their own components may extend these keys
 * at will. Thus, evgen plugins must be prepared to deal
 * with unknown keys by including them in their generated
 * output in an appropriate fashion */

 /* define some common subsystems for describing the
  * type of source that generated the event */
#define ORCM_COMPONENT_FABRIC       "orcm.comp.fab"     // fabric manager
#define ORCM_COMPONENT_DIAG         "orcm.comp.diag"    // system dianostics
#define ORCM_COMPONENT_PROV         "orcm.comp.prov"    // provisioning
#define ORCM_COMPONENT_IMG          "orcm.comp.img"     // image management
#define ORCM_COMPONENT_CONFIG       "orcm.comp.cfg"     // configuration management
#define ORCM_COMPONENT_MON          "orcm.comp.mon"     // system monitoring
#define ORCM_COMPONENT_DB           "orcm.comp.db"      // system management database
#define ORCM_COMPONENT_RM           "orcm.comp.rm"      // resource manager
#define ORCM_COMPONENT_IO           "orcm.comp.io"      // file system and IO services
#define ORCM_COMPONENT_OVLYNET      "orcm.comp.on"      // overlay network
#define ORCM_COMPONENT_CNK          "orcm.comp.cnk"     // compute node kernel
#define ORCM_COMPONENT_SMK          "orcm.comp.smk"     // system management node kernel
#define ORCM_COMPONENT_SEC          "orcm.comp.sec"     // security
#define ORCM_COMPONENT_INV          "orcm.comp.inv"     // inventory management

/* the following components are also physical locations, but
 * are provided here as components as they might best appear
 * in that form for different implementations */
#define ORCM_COMPONENT_CABINET      "orcm.comp.cab"     // compute/service cabinet
#define ORCM_COMPONENT_CDU          "orcm.comp.cdu"     // cooling distribution unit
#define ORCM_COMPONENT_CHASSIS      "orcm.comp.chas"    // chassis
#define ORCM_COMPONENT_CMM          "orcm.comp.cmm"     // chassis management module
#define ORCM_COMPONENT_BMC          "orcm.comp.bmc"     // baseboard management controller
#define ORCM_COMPONENT_PDU          "orcm.comp.pdu"     // power distribution unit
#define ORCM_COMPONENT_CBLADE       "orcm.comp.cbld"    // compute blade
#define ORCM_COMPONENT_SBLADE       "orcm.comp.sbld"    // switch blade
#define ORCM_COMPONENT_SWITCH       "orcm.comp.swtch"   // standalone switch

/* define some common non-physical sub-components */
#define ORCM_SUBCOMPONENT_FW        "orcm.sub.fw"       // firmware error
#define ORCM_SUBCOMPONENT_SW        "orcm.sub.sw"       // software error
#define ORCM_SUBCOMPONENT_PROC      "orcm.sub.proc"     // daemon or process error
#define ORCM_SUBCOMPONENT_BIOS      "orcm.sub.bios"     // BIOS error
#define ORCM_SUBCOMPONENT_AUTH      "orcm.sub.auth"     // authentication error
#define ORCM_SUBCOMPONENT_PERM      "orcm.sub.perm"     // authorization error
#define ORCM_SUBCOMPONENT_ENCRYPT   "orcm.sub.encrypt"  // encryption error

/* define some common physical sub-components */
#define ORCM_SUBCOMPONENT_TEMP      "orcm.sub.temp"     // temperature controller
#define ORCM_SUBCOMPONENT_VOLT      "orcm.sub.volt"     // voltage controller
#define ORCM_SUBCOMPONENT_FREQ      "orcm.sub.freq"     // frequency controller
#define ORCM_SUBCOMPONENT_MCA       "orcm.sub.mca"      // machine check register
#define ORCM_SUBCOMPONENT_PWR       "orcm.sub.pwr"      // power subsystem
#define ORCM_SUBCOMPONENT_FAN       "orcm.sub.fan"      // cooling fan
#define ORCM_SUBCOMPONENT_PCIE      "orcm.sub.pcie"     // PCIE controller or device
#define ORCM_SUBCOMPONENT_ETH       "orcm.sub.eth"      // management Ethernet
#define ORCM_SUBCOMPONENT_IB        "orcm.sub.ib"       // InfiniBand fabric
#define ORCM_SUBCOMPONENT_OMNI      "orcm.sub.omni"     // OmniPath fabric
#define ORCM_SUBCOMPONENT_TSC       "orcm.sub.tsc"      // TrueScale fabric
#define ORCM_SUBCOMPONENT_CBL       "orcm.sub.cbl"      // Cabling
#define ORCM_SUBCOMPONENT_CLK       "orcm.sub.clk"      // Clock
#define ORCM_SUBCOMPONENT_LUSTRE    "orcm.sub.lustre"   // Lustre parallel file system
#define ORCM_SUBCOMPONENT_PNS       "orcm.sub.pns"      // Panasas paralle file system
#define ORCM_SUBCOMPONENT_SWITCH    "orcm.sub.swtch"    // switch that is part of a component
#define ORCM_SUBCOMPONENT_PWRREC    "orcm.sub.pwrrec"   // power rectifier module
#define ORCM_SUBCOMPONENT_OC        "orcm.sub.oc"       // optical connectors
#define ORCM_SUBCOMPONENT_SWSB      "orcm.sub.swsb"     // switch board (e.g., in a switch blade)
#define ORCM_SUBCOMPONENT_NB        "orcm.sub.nb"       // node board in a blade
#define ORCM_SUBCOMPONENT_NBC       "orcm.sub.nbc"      // node board controller
#define ORCM_SUBCOMPONENT_BMC       "orcm.sub.bmc"      // embedded baseboard management controller
#define ORCM_SUBCOMPONENT_ACC       "orcm.sub.acc"      // accelerator card in node board
#define ORCM_SUBCOMPONENT_CLIN      "orcm.sub.clin"     // coolant intake (air or liquid)
#define ORCM_SUBCOMPONENT_CLOUT     "orcm.sub.clout"    // coolant outlet (air or liquid)
#define ORCM_SUBCOMPONENT_MEM       "orcm.sub.mem"      // ECC Memory sub-components

/* define some common physical locations - multiple combinations of
 * these can be provided to fully describe the hierarcy of the location */
#define ORCM_LOC_CLUSTER            "orcm.loc.clstr"    // identifier of the cluster
#define ORCM_LOC_ENC                "orcm.loc.enc"      // general enclosure identifier
#define ORCM_LOC_ROW                "orcm.loc.row"      // row identifier
#define ORCM_LOC_RACK               "orcm.loc.rack"     // rack identifier
#define ORCM_LOC_CABINET            "orcm.loc.cab"      // compute/service cabinet identifier
#define ORCM_LOC_CHASSIS            "orcm.loc.chas"     // chassis identifier
#define ORCM_LOC_BLADE              "orcm.loc.blade"    // blade identifier
#define ORCM_LOC_NODE               "orcm.loc.node"     // node identifier
#define ORCM_LOC_SOCKET             "orcm.loc.skt"      // socket identifier
#define ORCM_LOC_DIE                "orcm.loc.die"      // die identifier
#define ORCM_LOC_CORE               "orcm.loc.core"     // core identifier
#define ORCM_LOC_HWT                "orcm.loc.hwt"      // hardware-thread identifier
#define ORCM_LOC_DIMM               "orcm.loc.dimm"     // DIMM board

/* provide an ability to include job and session ID in the description - we
 * cannot predict the data type here, but they can specify
 * it in the value type */
#define ORCM_DESC_JOBID             "orcm.desc.jobid"   // ID of affected job (may be multiple entries)
#define ORCM_DESC_SESSION_ID        "orcm.desc.ssid"    // ID of affected sessions (may be multiple entries)

/* define some common descriptive keys */
#define ORCM_DESC_TEMP_HI           "orcm.desc.thi"     // high temperature
#define ORCM_DESC_TEMP_LO           "orcm.desc.tlo"     // low temperature
#define ORCM_DESC_FLOW_HI           "orcm.desc.fhi"     // high coolant flow rate (air or liquid)
#define ORCM_DESC_FLOW_LO           "orcm.desc.flo"     // low coolant flow rate (air or liquid)
#define ORCM_DESC_PRESSURE_HI       "orcm.desc.phi"     // high coolant pressure (air or liquid)
#define ORCM_DESC_PRESSURE_LO       "orcm.desc.plo"     // low coolant pressure (air or liquid)

/* Storage strategy - Provide the Ability to store events in different framework
 * */
#define ORCM_STORAGE_TYPE_DATABASE           0       //DATABASE FRAMEWORK
#define ORCM_STORAGE_TYPE_NOTIFICATION       1       //Notification FRAMEWORK
#define ORCM_STORAGE_TYPE_PUBSUB             2       //PubSub FRAMEWORK
#define ORCM_STORAGE_TYPE_UNDEFINED          3       //Undefined FRAMEWORK

/* Event fault category - In case of a fault, provide the ability to categorize the faults
 */
#define ORCM_EVENT_SOFT_FAULT        0   // Recoverable error
#define ORCM_EVENT_HARD_FAULT        1   // Non recoverable error
#define ORCM_EVENT_UNKOWN_FAULT      2   // Not defined


END_C_DECLS

#endif /* MCA_EVGEN_TYPES_H */
