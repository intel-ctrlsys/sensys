/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef EDAC_COLLECTOR_TESTS_H
#define EDAC_COLLECTOR_TESTS_H

#include "gtest/gtest.h"
#include "edac_tests_mocking.h"

#include <map>
#include <vector>
#include <queue>

// ORCM (Not from mocking)
#include "orcm/mca/db/db.h"

class ut_edac_collector_tests: public testing::Test
{
    protected: // gtest required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ClearBuffers();
        static void ResetTestEnvironment();

        // Mocking....
        static int Stat(const char* pathname, struct stat* sb);
        static int StatFail(const char* pathname, struct stat* sb);
        static FILE* FOpen(const char* path, const char* mode);
        static FILE* FOpenFail(const char* path, const char* mode);
        static ssize_t GetLine(char** line_buf, size_t* line_buff_size, FILE* fd);
        static ssize_t GetLineFail(char** line_buf, size_t* line_buff_size, FILE* fd);
        static int FClose(FILE* fd);
        static void OrteErrmgrBaseLog(int err, char* file, int lineno);
        static void OpalOutputVerbose(int level, int output_id, const char* line);
        static char* OrteUtilPrintNameArgs(const orte_process_name_t *name);
        static int OpalDssPack(opal_buffer_t* buffer, const void* src, int32_t num_vals, opal_data_type_t type);
        static int OpalDssUnpack(opal_buffer_t* buffer, void* dst, int32_t* num_vals, opal_data_type_t type);
        static void OrcmAnalyticsBaseSendData(orcm_analytics_value_t* data);
        static opal_event_base_t* OpalProgressThreadInit(const char* name);
        static int OpalProgressThreadFinalize(const char* name);
        static void MyDbStoreNew(int dbhandle, orcm_db_data_type_t data_type, opal_list_t *input,
                                 opal_list_t *ret, orcm_db_callback_fn_t cbfunc, void *cbdata);

        // Callbacks...
        static void ErrorSink(const char* pathname, int error_number, void* user_data);
        static void DataSink(const char* label, int count, void* user_data);
        static void InventorySink(const char* label, const char* name, void* user_data);

        static const char* hostname_;
        static const char* plugin_name_;
        static const char* proc_name_;
        static std::map<std::string,std::string> sysfs_;
        static unsigned int fopened_;
        static int last_errno_;
        static std::string last_error_filename_;
        static void* last_user_data_;
        static int last_orte_error_;
        static int fail_pack_on_;
        static int fail_pack_count_;
        static int fail_unpack_on_;
        static int fail_unpack_count_;
        static bool fail_pack_buffer_;
        static bool fail_unpack_buffer_;
        static std::map<std::string,int> logged_data_;
        static std::map<std::string,int> golden_data_;
        static std::map<std::string,std::string> logged_inv_;
        static std::map<std::string,std::string> golden_inv_;
        static std::vector<std::string> opal_output_verbose_;
        static std::queue<int32_t> packed_int32_;
        static std::queue<std::string> packed_str_;
        static std::queue<struct timeval> packed_ts_;
        static std::vector<orcm_analytics_value_t*> current_analytics_values_;
        static std::map<std::string,std::string> database_data_;
}; // class

#endif // EDAC_COLLECTOR_TESTS_H
