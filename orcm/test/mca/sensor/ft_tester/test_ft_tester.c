/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "opal/runtime/opal.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/sst/base/base.h"
#include "orte/mca/ess/base/base.h"
#include "orte/runtime/orte_globals.h"

#include "sensor_ft_tester.h"

/* ft_tester sensor test
*
* --omca sensor_ft_tester_fail_prob 0.0
* --omca sensor_ft_tester_multi_allowed 0.0
* --omca sensor_ft_tester_daemon_fail_prob 0.0
* --omca sensor_ft_tester_aggregator_fail_prob 0.0
*
* If configuration file says we are aggregator, and MCA parameter
* says aggregator fail probability is greater than zero, then 
* in ft_tester's sample() function an abort is called, resulting in exit(1).
*
* If configuration file says we are daemon, and MCA parameter
* says daemon fail probability is greater than zero, then 
* in ft_tester's sample() function an abort is called, resulting in exit(1).
*
* Test for "fail_prob" for termination of child processes is not implemented.
* If "fail_prob" is greater than zero, then in ft_tester's sample() function
* a SIGTERM is sent to a child process.  If "multi_allowed" is 1, another
* child process may will be killed eventually as well.
*
* Return values:
*
* 0   success
* 1   test failed
* 77  test skipped
* 99  test error (not an error of code under test)
*/

extern opal_list_t *orcm_clusters;
extern opal_list_t *orcm_schedulers;

static int idx;
static short awaitKill=0;
static float fail_prob = 0.0;
static float daemon_fail_prob = 0.0;
static float aggregator_fail_prob = 0.0;
static short multi_allowed = 0;
static mca_base_component_t *sensorInfo = &mca_sensor_ft_tester_component.super.base_version;
extern orcm_sensor_base_module_t orcm_sensor_ft_tester_module;

static void getCommandLineArgs(void)
{
  char *optionValue=NULL;
  (void) mca_base_component_var_register (sensorInfo, "fail_prob", NULL,
    MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0, OPAL_INFO_LVL_9,
    MCA_BASE_VAR_SCOPE_READONLY, &optionValue);

  if (optionValue != NULL)
    fail_prob=atof(optionValue);

  (void) mca_base_component_var_register (sensorInfo, "multi_allowed", NULL,
    MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0, OPAL_INFO_LVL_9,
    MCA_BASE_VAR_SCOPE_READONLY, &multi_allowed);

  optionValue = NULL;
  (void) mca_base_component_var_register (sensorInfo, "daemon_fail_prob", NULL,
    MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0, OPAL_INFO_LVL_9,
    MCA_BASE_VAR_SCOPE_READONLY, &optionValue);

  if (optionValue != NULL)
    daemon_fail_prob=atof(optionValue);

  optionValue = NULL;
  (void) mca_base_component_var_register (sensorInfo, "aggregator_fail_prob", NULL,
    MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0, OPAL_INFO_LVL_9,
    MCA_BASE_VAR_SCOPE_READONLY, &optionValue);

  if (optionValue != NULL)
    aggregator_fail_prob=atof(optionValue);
}

static int setupOrcm(void)
{
  int ret;
  /* Call enough of orcm/orte so that the configuration file is
   * read and we know if we are an aggregator or a compute node daemon.
   */

  if (ORTE_SUCCESS != (ret = opal_init(NULL, NULL))) {
    printf("FAIL Error in opal_init()\n");
    return 1;
  }

  orte_process_info.proc_type = ORCM_DAEMON;
  if (ORTE_SUCCESS != (ret = orte_proc_info())) {
    printf("FAIL Error in orte_proc_info()\n");
    return 1;
  }   
  orte_event_base = opal_sync_event_base;
  orcm_clusters = OBJ_NEW(opal_list_t);
  orcm_schedulers = OBJ_NEW(opal_list_t);

  if (ORCM_SUCCESS != (ret = mca_base_framework_open(&orcm_cfgi_base_framework, 0))) {
    printf("FAIL orcm_cfgi_base_open\n");
    return 1;
  }
  if (ORCM_SUCCESS != (ret = orcm_cfgi_base_select())) {
    printf("FAIL orcm_cfgi_select\n");  /* bad configuration file */
    return 99;
  }

  if (ORCM_SUCCESS != (ret = mca_base_framework_open(&orcm_sst_base_framework, 0))) {
    printf("FAIL orcm_sst_base_framework\n");
    return 1;
  }
  if (ORCM_SUCCESS != (ret = orcm_sst_base_select())) {
    printf("FAIL orcm_sst_base_select\n");
    return 1;
  }

  /* We need to set up the ESS framework because when ft_tester kills
   * itself it calls the abort function.
   */

  if (ORTE_SUCCESS != (ret = mca_base_framework_open(&orte_ess_base_framework, 0))) {
    printf("FAIL orte_ess_base_open\n");
    return 1;
  }
  if (ORTE_SUCCESS != (ret = orte_ess_base_select())) {
    printf("FAIL orte_ess_base_select\n");
    return 1;
  }
  if (ORTE_SUCCESS != (ret = orte_ess.init())) {
    printf("FAIL orte_ess_init\n");
    return 1;
  }

  return 0;
}

static void showDot(void)
{
  static int columnCount=0;

  columnCount++;
  printf(".");

  if (columnCount == 70){
    printf("\n");
    columnCount=0;
  }

  fflush(stdout);
}

int main(int argc, char *argv[])
{
  int ret;
  int status;
  int exitStatus;
  int numCalls;
  char *role;
  opal_cmd_line_t cmd_line;
  mca_base_module_t *module;
  int priority;
  orcm_sensor_sampler_t sampler;
  short agg_proc;
  short cn_proc;
  short failed;
  pid_t procNum;

  printf("Test of sensor ft_tester.\n");

  /* Command line arguments *******************************/

  opal_cmd_line_create(&cmd_line,NULL);
  mca_base_cmd_line_setup(&cmd_line);
  ret = opal_cmd_line_parse(&cmd_line, false, argc, argv);

  if (ret != ORCM_SUCCESS){
    printf("FAIL parsing command line\n");
    return 99;
  }

  ret = mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);

  if (ret != ORCM_SUCCESS){
    printf("NOT RUN error parsing command line\n");
    return 99;
  }

  getCommandLineArgs();

  printf("sensor_ft_tester_fail_prob %f\n",fail_prob);
  printf("sensor_ft_tester_multi_allowed %d\n",multi_allowed);
  printf("sensor_ft_tester_daemon_fail_prob %f\n",daemon_fail_prob);
  printf("sensor_ft_tester_aggregator_fail_prob %f\n",aggregator_fail_prob);

  if (fail_prob > 0.0){
    printf(
    "NOT RUN Testing the termination of child process not implemented yet.\n");
    return 77;
  }

  /* Aggregator or compute node daemon? *******************/

  agg_proc = cn_proc = 0;

  ret = setupOrcm();

  if (ret > 0){
    return ret;
  }

  if (ORCM_PROC_IS_DAEMON){
    role = "cn daemon";
    cn_proc = 1;
  }
  else if (ORCM_PROC_IS_AGGREGATOR){
    role = "aggregator";
    agg_proc = 1;
  }
  else{
    printf(
      "NOT RUN Can not determine whether test is aggregator or cn daemon.\n");
    return 99;
  }

  printf("Role %s\n", role);

  if (agg_proc && aggregator_fail_prob > 0.0){
    awaitKill = 1;
  }
  else if (cn_proc && daemon_fail_prob > 0.0){ 
    awaitKill = 1;
  }

  /* Invoke ft_tester interface  **************************/

  /* **register**: Get the values of the ft_tester command line parameters  */

  ret = sensorInfo->mca_register_component_params();

  if (ret != ORCM_SUCCESS){
    printf("FAIL Error in register()\n");
    return 1;
  }

  /* **open**: Process the ft_tester command line parameters */

  ret = sensorInfo->mca_open_component();

  if (ret != ORCM_SUCCESS){
    printf("FAIL Error in open()\n");
    return 1;
  }

  if (mca_sensor_ft_tester_component.fail_prob != fail_prob){
    printf("FAIL Error in open()\n");
    return 1;
  }

  if (mca_sensor_ft_tester_component.daemon_fail_prob != daemon_fail_prob){
    printf("FAIL Error in open()\n");
    
  }

  if (mca_sensor_ft_tester_component.aggregator_fail_prob != 
    aggregator_fail_prob){
    printf("FAIL Error in open()\n");
    return 1;
  }

  if (mca_sensor_ft_tester_component.multi_fail != multi_allowed){
    printf("FAIL Error in open()\n");
    return 1;
  }

  /* **query** : If any of the probabilities were greater than zero, a non-NULL
   *        module should be returned and priority should be greater than zero.
   *        Otherwise module is NULL, priority is zero.
   *
   * Returns an error when it doesn't want to load.  So don't check for
   * success.
   */

  ret = sensorInfo->mca_query_component(&module, &priority);

  if (daemon_fail_prob > 0.0 || aggregator_fail_prob > 0.0){
    if (module == NULL || priority == 0){
      printf("FAIL Error in query().  ft_tester should instantiate itself.\n");
      return 1;
    }
  }
  else{
    if (module != NULL ||  priority > 0){
      printf("FAIL Error in query(). ft_tester should *not* instantiate.\n");
      return 1;
    }
  }

  failed = 0;

  if (priority > 0){

    /* **sample**: If the probability of my process being killed was
     *         greater than zero, then after some number of calls
     *         to sample, sample should abort the process (exit(1)).
     */

    exitStatus = 0;

    if (daemon_fail_prob > 0.0){
      numCalls = (int)(10.0 / daemon_fail_prob);
    }
    else{
      numCalls = (int)(10.0 / aggregator_fail_prob);
    }

    printf("Calling ft_tester sample function %d times\n",numCalls);

    fflush(stdout);   /* child process also printed stdout buffer */

    procNum = fork();

    if (procNum > 0){
      /* Parent: Did subprocess get killed? */

      wait(&status);
      exitStatus = WEXITSTATUS(status);
 
      if (exitStatus > 0){
        printf("\na call to ft_tester status() killed the process\n");
        if (!awaitKill){
          failed = 1;
        }
      }
      else{
        printf("\nft_tester status() never killed the process\n");
        if (awaitKill){
          failed = 1;
        }
      }
    }
    else if (procNum == 0){

      for (idx=0; idx < numCalls; idx++){
        showDot();
        orcm_sensor_ft_tester_module.sample(&sampler);
      }
      exit(0);

    }
    else{
      printf("NOT RUN Test error in fork()\n");
      return 99;
    }
  }

  /* **close** ft_tester */

  ret = sensorInfo->mca_close_component();

  if (ret != ORCM_SUCCESS){
    printf("FAIL Error in close()\n");
    return 1;
  }

  if (failed){
    printf("FAIL\n");
    return 1;
  }
  else{
    printf("PASS\n");
  }
  
  return 0;
}

