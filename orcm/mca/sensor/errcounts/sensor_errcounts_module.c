#include "sensor_errcounts.h"

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_errcounts_module = {
    errcounts_init_relay,
    errcounts_finalize_relay,
    errcounts_start_relay,
    errcounts_stop_relay,
    errcounts_sample_relay,
    errcounts_log_relay,
    errcounts_inventory_collect_relay,
    errcounts_inventory_log_relay,
    errcounts_set_sample_rate_relay,
    errcounts_get_sample_rate_relay,
    errcounts_enable_sampling_relay,
    errcounts_disable_sampling_relay,
    errcounts_reset_sampling_relay
};
