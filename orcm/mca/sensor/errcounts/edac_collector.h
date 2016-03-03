/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef EDAC_COLLECTOR_H
#define EDAC_COLLECTOR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    #include <stdbool.h>

    typedef void (*edac_error_callback_fn_t)(const char* pathname, int error_number, void* user_data);
    typedef void (*edac_data_callback_fn_t)(const char* label, int error_count, void* user_data);
    typedef void (*edac_inventory_callback_fn_t)(const char* label, const char* name, void* user_data);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus

#include <string>

#ifdef GTEST_MOCK_TESTING
#define PRIVATE public
#else
#define PRIVATE private
#endif

class edac_collector
{
    public:
        edac_collector(edac_error_callback_fn_t error_cb = NULL, const char* edac_path = NULL);
        ~edac_collector();

        bool collect_data(edac_data_callback_fn_t cb, void* user_data);
        bool collect_inventory(edac_inventory_callback_fn_t cb, void* user_data);

        bool have_edac() const;

    PRIVATE:

        int get_mc_folder_count() const;
        int get_csrow_folder_count(int mc) const;
        int get_channel_folder_count(int mc, int csrow) const;

        int get_xx_count(int mc, int csrow, const char* filename) const;
        int get_ue_count(int mc, int csrow) const;
        int get_ce_count(int mc, int csrow) const;

        int get_channel_ce_count(int mc, int csrow, int channel) const;
        std::string get_channel_label(int mc, int csrow, int channel) const;

        void report_error(const char* pathname, int error_number) const;
        void log_data(const char* label, int count, edac_data_callback_fn_t cb) const;
        void log_inventory(const char* label, const char* name, edac_inventory_callback_fn_t cb) const;

        std::string& remove_newlines(std::string& str) const;

        // Fields...
        std::string base_edac_path;
        edac_error_callback_fn_t error_callback;
        unsigned long long user_data_;
};

#undef PRIVATE
#endif /* __cplusplus */

#endif /* EDAC_COLLECTOR_H */
