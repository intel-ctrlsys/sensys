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
#define GET_ACPI_POWER           (0x07 | (NETFN_APP << 8))
// The total number of Compute Nodes/Child nodes present under each aggregator
#define TOTAL_NODES 2
// The total number of parameters that need to be gathered for each node
// Eventually this number should equal the items under ipmi_property_t
#define TOTAL_PROPERTIES_PER_NODE   14
// The total number of IPMI Calls that need to be called for each node
#define TOTAL_CALLS_PER_NODE        2

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

// IPMI Properties (To be deprecated)
struct ipmi_properties{
    char node_name[16];
    int bmc_rev[2];
    int ipmi_ver[2];    
    int man_id[8];
    char sys_power_state[10];
    char dev_power_state[10];
    struct ipmi_properties *next;
};

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
    unsigned char ccode;
} ipmi_node_details_t;

// Node Capabilities
unsigned char capability;
// Node Properties
typedef struct {
    char bmc_rev[16];
    char ipmi_ver[16];
    char man_id[16];
    char baseboard_serial[16];
    char sys_power_state[16];
    char dev_power_state[16];
    float ps1_usage;
    float ps1_temp;
    float ps2_usage;
    float ps2_temp;
    float fan1_cpu_rpm;
    float fan2_cpu_rpm;
    float fan1_sys_rpm;
    float fan2_sys_rpm;
} ipmi_properties_t;


typedef struct {
    // Node Details
    ipmi_node_details_t node;
    // Node Capabilities
    unsigned char capability[TOTAL_PROPERTIES_PER_NODE];
    // Node Properties
    ipmi_properties_t prop;
} ipmi_capsule_t;


typedef struct {
    char node_name[64];
    char host_ipaddr[16];
    char bmc_ipaddr[16];
    char username[16];
    char password[16];
    struct orcm_sensor_hosts_t *next;
}orcm_sensor_hosts_t;


// List of all properties to be scanned by the IPMI Plugin
// This total number should correspond to the value TOTAL_PROPERTIES_PER_NODE!!
typedef enum {
    BMC_REV = 0,
    IPMI_VER,
    MAN_ID,
    BASEBOARD_SERIAL,
    SYS_POWER_STATE,
    DEV_POWER_STATE,
    PS1_USAGE,
    PS1_TEMP,
    PS2_USAGE,
    PS2_TEMP,
    FAN1_CPU_RPM,
    FAN2_CPU_RPM,
    FAN1_SYS_RPM,
    FAN2_SYS_RPM
} ipmi_property_t;

// Function Declarations
void get_system_power_state(uchar in, char* str);
void get_device_power_state(uchar in, char* str);

int ipmi_exec_call(ipmi_capsule_t *cap);

#endif
