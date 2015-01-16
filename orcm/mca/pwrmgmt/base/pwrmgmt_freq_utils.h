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

#ifndef ORCM_MCA_PWRMGMT_FREQ_UTILS_H
#define ORCM_MCA_PWRMGMT_FREQ_UTILS_H

#include "orcm_config.h"
#include "orcm/constants.h"

BEGIN_C_DECLS

/**
 * initialize the frequency/governor tracker
 *
 * Creates tracking objects and initializes them with the 
 * current system information
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORTE_ERROR   An unspecified error occurred
 */
int orcm_pwrmgmt_freq_init(void);

/**
 * finalize the frequency/governor tracker
 *
 * Cleanup tracking objects set the system setting back to default
 */
void orcm_pwrmgmt_freq_finalize(void);

/**
 * get current number of cpus
 *
 * returns the number of online cpus as reported by the OS
 *
 * @retval number of reported cpus
 */
int orcm_pwrmgm_freq_get_num_cpus(void);

/**
 * Set the scaling governor for a cpu
 *
 * Set the scaling governor for a specified cpu
 *
 * @param[in] cpu - the target cpu (set to -1 to affect all cpus)
 * @param[in] governor - a string containing the governor name
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 * @retval ORTE_ERROR   An unspecified error occurred
 */
int orcm_pwrmgmt_freq_set_governor(int cpu, char* governor);

/**
 * Set the maximum frequency for a cpu
 *
 * Set the maximum frequency for a specified cpu (set to -1 to affect all cpus)
 *
 * @param[in] cpu - the target cpu
 * @param[in] freq - the requested frequency
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 * @retval ORTE_ERROR   An unspecified error occurred
 */
int orcm_pwrmgmt_freq_set_max_freq(int cpu, float freq);

/**
 * Set the minimum frequency for a cpu
 *
 * Set the minimum frequency for a specified cpu (set to -1 to affect all cpus)
 *
 * @param[in] cpu - the target cpu
 * @param[in] freq - the requested frequency
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 * @retval ORTE_ERROR   An unspecified error occurred
 */
int orcm_pwrmgmt_freq_set_min_freq(int cpu, float freq);

/**
 * Get the list of supported governors for a cpu
 *
 * Get the list of supported governors for a specified cpu
 *
 * @param[in] cpu - the target cpu
 * @param[out] governors - a comma seperated list of supported governors
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 */
int  orcm_pwrmgmt_freq_get_supported_governors(int cpu, opal_list_t** governors);

/**
 * Get the list of supported frequencies for a cpu
 *
 * Get the list of supported frequencies for a specified cpu
 *
 * @param[in] cpu - the target cpu
 * @param[out] frequencies - an array of supported frequencies
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 */
int  orcm_pwrmgmt_freq_get_supported_frequencies(int cpu, opal_list_t** frequencies);

/**
 * Set the system back to its initial state
 *
 * Set the governor and max/min frequencies back to their initial values
 * for all cpus
 *
 * @retval ORTE_SUCCESS Success
 * @retval ORCM_ERR_NOT_INITIALIZED init could not be completed
 * @retval ORTE_ERROR   An unspecified error occurred
 */
int orcm_pwrmgmt_freq_reset_system_settings(void);

END_C_DECLS

#endif
