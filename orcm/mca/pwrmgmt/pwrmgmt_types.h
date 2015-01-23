/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/** @file:
 */

#ifndef ORCM_MCA_PWRMGMT_TYPES_H
#define ORCM_MCA_PWRMGMT_TYPES_H

#include "orcm_config.h"
#include "orcm/constants.h"

BEGIN_C_DECLS

/*
 * ORCM Power Management Modes
 *
 * ORCM_PWRMGMT_MODE_NONE
 * - No power management. Do nothing.
 *
 * ORCM_PWRMGMT_MODE_MANUAL_FREQ
 * - Takes a frequency and hard sets all nodes/cpus to that fixed frequency.
 *
 * The following nodes all operate under a per allocation power cap:
 *
 * ORCM_PWRMGMT_MODE_AUTO_UNIFORM_FREQ
 * - Ensures that all cpus in the allocation run at the same frequency. The frequency can change
 *   depending on the workload characteristics as long as all cpus stay in synch creating a 
 *   global frequency. This can be done on a time quantum basis or in a continuous manner.
 *
 * ORCM_PWRMGMT_MODE_AUTO_DUAL_UNIFORM_FREQ
 * - This mode is still being defined. However, the general idea is to set some group of cores
 *   in charge of running parallel portions of code to ORCM_PWRMGMT_MODE_AUTO_UNIFORM_FREQ.
 *   Another set of cores in charge of running serial code would run at their max supported
 *   frequency. It may be that the serial cores are a subset of the parallel cores and they
 *   switch modes as they go into serial regions.
 *
 * ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_PERF
 * - This is the global energy optimization mode. In this mode cpus in the allocation can all
 *   run at different frequencies in order to optimize global performance under a power cap.
 *
 * ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_EFFIENCY
 * - This is the global energy optimization mode. In this mode cpus in the allocation can all
 *   run at different frequencies in order to optimize global efficiency under a power cap.
 */

#define ORCM_PWRMGMT_MODE_NONE                          0
#define ORCM_PWRMGMT_MODE_MANUAL_FREQ                   1
#define ORCM_PWRMGMT_MODE_AUTO_UNIFORM_FREQ             2
#define ORCM_PWRMGMT_MODE_AUTO_DUAL_UNIFORM_FREQ        3
#define ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_PERF        4
#define ORCM_PWRMGMT_MODE_AUTO_GEO_GOAL_MAX_EFFICIENCY  5
/* Add any new modes above here and then fix the ORCM_PWRMGMT_NUM_MODES to be
 * the maximum+1 of all the modes. The modes numbering cannot have gaps.
 */
#define ORCM_PWRMGMT_NUM_MODES                          6


#define ORCM_PWRMGMT_MAX_FREQ                           255.0
#define ORCM_PWRMGMT_MIN_FREQ                           256.0

END_C_DECLS

#endif
