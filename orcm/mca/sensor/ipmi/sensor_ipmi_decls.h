/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * IPMI resource manager sensor 
 */
#ifndef ORTE_SENSOR_IPMI_DECLS_H
#define ORTE_SENSOR_IPMI_DECLS_H

#define uchar unsigned char
#define GET_ACPI_POWER          (0x07 | (NETFN_APP << 8))
#define GET_BMC_IP_CMD          0x03
// The total number of Compute Nodes/Child nodes present under each aggregator
#define TOTAL_NODES 2
// The total number of parameters that need to be gathered for each node
// Eventually this number should equal the items under ipmi_property_t
#define TOTAL_PROPERTIES_PER_NODE   14
#define TOTAL_FLOAT_METRICS     50
#define MAX_UNIT_LENGTH         20
#define MAX_METRIC_NAME         20
// The total number of IPMI Calls that need to be called for each node
#define TOTAL_CALLS_PER_NODE        2
#define MAX_FRU_DEVICES  254

unsigned char disable_ipmi;

// GET_DEVICE_ID
typedef struct {
    uchar   dev_id;
    uchar   dev_rev;
    uchar   fw_rev_1;
    uchar   fw_rev_2;
    uchar   ipmi_ver;
    uchar   dev_support;
    uchar   manufacturer_id[2];
    uchar   product_id [2];
    uchar   aux_fw_rev[4];
} _device_id_t;

typedef union {
    uchar           raw[16];
    _device_id_t    bits;
} device_id_t;

// GET_ACPI_STATE
typedef struct {
    uchar   sys_power_state;
    uchar   dev_power_state;
} _acpi_power_state_t;

typedef union {
    uchar   raw[3];
    _acpi_power_state_t bits;
} acpi_power_state_t;

// Node Details
typedef struct {
    char name[64];
    char bmc_ip[16];
    char host_ip[16];
    char device_id[16];
    char user[16];
    char pasw[16];
    int auth;
    int priv;
    int ciph;
} ipmi_node_details_t;

// Node Properties
typedef struct {
    char bmc_rev[16];
    char ipmi_ver[16];
    char man_id[16];
    char baseboard_serial[16];
    char sys_power_state[16];
    char dev_power_state[16];
    int total_metrics;
    /* Metric string identifiers*/
    char metric_label[TOTAL_FLOAT_METRICS][MAX_METRIC_NAME];

    float collection_metrics[TOTAL_FLOAT_METRICS];  /* Array to store all non-string metrics */
    char collection_metrics_units[TOTAL_FLOAT_METRICS][MAX_UNIT_LENGTH]; /* Array to store units for all non-string metrics */
} ipmi_properties_t;

typedef struct {
    // Node Details
    ipmi_node_details_t node;
    // Node Capabilities
    unsigned char capability[TOTAL_PROPERTIES_PER_NODE];
    // Node Properties
    ipmi_properties_t prop;
} ipmi_capsule_t;

typedef struct _orcm_sensor_hosts_t {
    ipmi_capsule_t  capsule;
    struct _orcm_sensor_hosts_t *next;
}orcm_sensor_hosts_t;

// List of all properties to be scanned by the IPMI Plugin
// This total number should correspond to the value TOTAL_PROPERTIES_PER_NODE!!
typedef enum {
    /* Add non-string numeric metrics under this section */
    PS1_USAGE = 0,
    PS1_TEMP = 1,
    PS2_USAGE = 2,
    PS2_TEMP = 3,
    FAN1_CPU_RPM = 4,
    FAN2_CPU_RPM = 5,
    FAN1_SYS_RPM = 6,
    FAN2_SYS_RPM = 7,

    /*Add string/character metrics under this section */
    BMC_REV = 8,
    IPMI_VER = 9,
    MAN_ID = 10,
    BASEBOARD_SERIAL = 11,
    SYS_POWER_STATE = 12,
    DEV_POWER_STATE = 13,    
} ipmi_property_t;

// Function Declarations
void orcm_sensor_ipmi_get_system_power_state(uchar in, char* str);
void orcm_sensor_ipmi_get_device_power_state(uchar in, char* str);
int orcm_sensor_ipmi_get_bmc_cred(orcm_sensor_hosts_t *host);
int orcm_sensor_ipmi_found(char *nodename);
unsigned int orcm_sensor_ipmi_counthosts(void);
int orcm_sensor_ipmi_addhost(char *nodename, char *host_ip, char *bmc_ip);
void orcm_sensor_ipmi_exec_call(ipmi_capsule_t *cap);
int orcm_sensor_ipmi_label_found(char * tag);
void orcm_sensor_get_fru_inv(void);
void orcm_sensor_get_fru_data(int id, long int fru_area);
#endif
