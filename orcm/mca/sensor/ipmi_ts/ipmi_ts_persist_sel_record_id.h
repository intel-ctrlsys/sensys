/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_TS_PERSIST_SEL_RECORD_ID_H
#define IPMI_TS_PERSIST_SEL_RECORD_ID_H

#include "ipmi_ts_sel_callback_defs.h"
#include <stdint.h>
#include <string>
#include <fstream>

class ipmi_ts_persist_sel_record_id
{
    public: // Public API
        ipmi_ts_persist_sel_record_id(const char* hostname, sel_error_callback_fn_t error_callback); // Agnostic constructor
        virtual ~ipmi_ts_persist_sel_record_id();

        virtual void save_latest_record_id();
        void set_record_id(uint16_t record_id);
        uint16_t get_record_id() const;
        virtual void load_last_record_id(const char* name);

    protected: // access for derived classes not using file IO...
        static const int MAX_LINE_SIZE;
        std::string hostname_;
        uint16_t record_id_;
        bool modified_;
        sel_error_callback_fn_t error_callback_;
        std::string storage_;

    private: // storage_ as a file: implementation variables and methods...
        void set_record_id_from_str(const std::string& name_value_str);
        void report_error(int level, const std::string& msg) const;
        std::string make_temp_filename() const;
        bool update_original_file(const char* tmp_name);
        bool copy_with_replace(std::ifstream& in_strm, std::ofstream& out_strm) const;
        bool create_new(const char* filename) const;
        void str_trim(std::string& buffer) const;
        void str_equals_split(const std::string& in_str, std::string& out_left, std::string& out_right) const;
        bool check_name_for_null_or_empty(const char* name);
        std::string get_stream_line(std::ifstream& strm) const;
};

#endif // PERSIST_SEL_RECORD_ID_H
