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
 * MCEDATA resource manager sensor 
 */
#ifndef ORCM_SENSOR_MCEDATA_H
#define ORCM_SENSOR_MCEDATA_H

#include "orcm_config.h"

#include "orcm/mca/sensor/sensor.h"

BEGIN_C_DECLS

#define     MCE_REG_COUNT   5

#define     MCG_STATUS  0
#define     MCG_CAP     1
#define     MCI_STATUS  2
#define     MCI_ADDR    3
#define     MCI_MISC    4

/* MCG_CAP Register Masks */
#define MCG_LMCE_P_MASK         ((uint64_t)1<<27)
#define MCG_ELOG_P_MASK         ((uint64_t)1<<26)
#define MCG_EMC_P_MASK          ((uint64_t)1<<25)
#define MCG_SER_P_MASK          ((uint64_t)1<<24)
#define MCG_EXT_CNT_MASK        ((uint64_t)0xff<<16)
#define MCG_TES_P_MASK          ((uint64_t)1<<11)
#define MCG_CMCI_P_MASK         ((uint64_t)1<<10)
#define MCG_EXT_P_MASK          ((uint64_t)1<<9)
#define MCG_CTL_P_MASK          ((uint64_t)1<<8)
#define MCG_BANK_COUNT_MASK     ((uint64_t)0xFF)

/*MCA Error Codes in MCi_STATUS register [15:0]*/
#define GEN_CACHE_ERR_MASK  (1<<3)
#define TLB_ERR_MASK        (1<<4)
#define MEM_CTRL_ERR_MASK   (1<<7)
#define CACHE_ERR_MASK      (1<<8)
#define BUS_IC_ERR_MASK     (1<<11)

/*MCi_STATUS Register masks */
#define MCI_VALID_MASK          ((uint64_t)1<<63)
#define MCI_OVERFLOW_MASK       ((uint64_t)1<<62)
#define MCI_UC_MASK             ((uint64_t)1<<61)
#define MCI_EN_MASK             ((uint64_t)1<<60)
#define MCI_MISCV_MASK          ((uint64_t)1<<59)
#define MCI_ADDRV_MASK          ((uint64_t)1<<58)
#define MCI_PCC_MASK            ((uint64_t)1<<57)
#define MCI_S_MASK              ((uint64_t)1<<56)
#define MCI_AR_MASK             ((uint64_t)1<<55)
#define EVENT_FILTERED_MASK     ((uint64_t)1<<12) /* Sec. 15.9.2.1 */

/* MCi_STATUS health Status tracking */
#define HEALTH_STATUS_MASK      ((uint64_t)0x3<<53)

/* MCi_MISC Register masks */
#define MCI_ADDR_MODE_MASK      (0x3<<6)
#define MCI_RECV_ADDR_MASK      (0x3F)

typedef enum _mcetype {
    e_gen_cache_error,
    e_tlb_error,
    e_mem_ctrl_error,
    e_cache_error,
    e_bus_ic_error,
    e_unknown_error
} mcetype;

typedef struct {
    orcm_sensor_base_component_t super;
    bool collect_cache_errors;
    char *logfile;
    bool use_progress_thread;
    int sample_rate;
    opal_event_base_t *ev_base;
    bool ev_active;
} orcm_sensor_mcedata_component_t;



ORCM_MODULE_DECLSPEC extern orcm_sensor_mcedata_component_t mca_sensor_mcedata_component;
extern orcm_sensor_base_module_t orcm_sensor_mcedata_module;


END_C_DECLS

#endif
