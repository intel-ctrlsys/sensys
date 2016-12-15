/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_ts_persist_sel_record_id.h"

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

using namespace std;

/* data will be less than this; format is 'hostname=last_record_id_16bit' */
const int ipmi_ts_persist_sel_record_id::MAX_LINE_SIZE = 80;

/* File must be accessed only one at a time in a threaded application */
static pthread_mutex_t save_mutex = PTHREAD_MUTEX_INITIALIZER;


//////////////////////////////////////////////////////////////////////////////
// Public API
ipmi_ts_persist_sel_record_id::ipmi_ts_persist_sel_record_id(const char* hostname,
                                             sel_error_callback_fn_t error_callback = NULL) :
    hostname_(hostname), record_id_(0), modified_(false),
    error_callback_(error_callback), storage_("")
{
}

ipmi_ts_persist_sel_record_id::~ipmi_ts_persist_sel_record_id()
{
    save_latest_record_id();
}

void ipmi_ts_persist_sel_record_id::save_latest_record_id()
{
    if("" == storage_) {
        return;
    }
    if(true == modified_) {
        string tmp_filename = make_temp_filename();
        if(true == tmp_filename.empty()) {
            report_error(0, "Unable to make a temporary filename used to rewrite the new record_id");
            return;
        }
        pthread_mutex_lock(&save_mutex); /* One thread access ONLY for file */
        ifstream old_strm(storage_.c_str());
        if(true == old_strm.fail()) { /* No existing file */
            create_new(storage_.c_str());
            modified_ = false;
        } else { /* existing file */
            ofstream tmp_strm(tmp_filename.c_str());
            if(true == tmp_strm.fail()) {
                report_error(0, "Unable to open temporary filename for write");
                if(false == old_strm.fail()) {
                    old_strm.close();
                }
            } else {
                if(true == copy_with_replace(old_strm, tmp_strm)) {
                    tmp_strm.close();
                    if(true == update_original_file(tmp_filename.c_str())) {
                        modified_ = false;
                    }
                }
            }
        }
        pthread_mutex_unlock(&save_mutex);
    }
}

void ipmi_ts_persist_sel_record_id::load_last_record_id(const char* name)
{

    if(false == check_name_for_null_or_empty(name)) {
        return;
    }
    storage_ = name;
    ifstream strm(storage_.c_str());
    if(true == strm.fail()) {
        return; /* Run for the first time; record_id_ should remain its default of zero (0) */
    }
    while(false == strm.eof()) {
        string line = get_stream_line(strm);
        if(false == line.empty()) {
            string name;
            string val;
            str_equals_split(line, name, val);
            if(name == hostname_) {
                set_record_id_from_str(val);
                break;
            }
        }
    }
    strm.close();
}

void ipmi_ts_persist_sel_record_id::set_record_id(uint16_t record_id)
{
    if(record_id != record_id_) {
        record_id_ = record_id;
        modified_ = true;
    }
}

uint16_t ipmi_ts_persist_sel_record_id::get_record_id() const
{
    return record_id_;
}


//////////////////////////////////////////////////////////////////////////////
// Private Implementation
bool ipmi_ts_persist_sel_record_id::check_name_for_null_or_empty(const char* name)
{
    if(NULL == name) {
        return false;
    }
    string sname = name;
    str_trim(sname);
    if("" == sname) {
        return false;
    }
    return true;
}

std::string ipmi_ts_persist_sel_record_id::get_stream_line(std::ifstream& strm) const
{
    char line_buffer[MAX_LINE_SIZE];
    strm.getline((char*)line_buffer, sizeof(line_buffer) - 1);
    line_buffer[sizeof(line_buffer) - 1] = '\0';
    string line = (char*)line_buffer;
    str_trim(line);

    return line;
}

void ipmi_ts_persist_sel_record_id::set_record_id_from_str(const std::string& name_value_str)
{
    size_t s_pos = name_value_str.find_first_of("0123456789");
    size_t e_pos = name_value_str.find_last_of("0123456789");
    string number = name_value_str.substr(s_pos, e_pos-s_pos+1); /* overflow (npos-npos+1 == 0) will result in empty 'number' */

    record_id_ = (uint16_t)strtol(number.c_str(), NULL, 10); /* If illformed will return zero (0) */
}

void ipmi_ts_persist_sel_record_id::report_error(int level, const std::string& msg) const
{
    string new_msg = hostname_ + ": " + msg + ": " + storage_;
    if(NULL != error_callback_) {
        error_callback_(level, msg.c_str());
    }
}

std::string ipmi_ts_persist_sel_record_id::make_temp_filename() const
{
    for(uint16_t i = 1; i < 4096; ++i) {
        char buffer[4]; // 12 bit unsigned hex number plus '\0'
        snprintf(buffer, sizeof(buffer), "%03x", i);
        string tmp_filename = storage_ + "." + (char*)buffer + ".tmp";
        FILE* f;
        if(NULL == (f = fopen(tmp_filename.c_str(), "r"))) {
            return tmp_filename;
        }
        fclose(f);
    }
    return string("");
}

bool ipmi_ts_persist_sel_record_id::update_original_file(const char* tmp_name)
{
    string backup = storage_ + ".backup";
    string new_file = storage_ + ".new";
    remove(backup.c_str());
    bool have_error = false;
    if(0 == rename(storage_.c_str(), backup.c_str())) {
        if(0 != rename(tmp_name, storage_.c_str())) {
            if(0 != rename(backup.c_str(), storage_.c_str())) {
                report_error(0, "Failed to restore backup file to original file; cannot recover user intervention required");
                have_error = true;
            }
        }
    } else {
        report_error(1, "Failed to backup original file; aborting file update; original file is ok; new file renamed with .new suffix");
        have_error = true;
    }
    if(true == have_error) {
        remove(new_file.c_str());
        rename(tmp_name, new_file.c_str());
        return false;
    }
    return true;
}

void ipmi_ts_persist_sel_record_id::str_trim(std::string& str) const
{
    const std::string whiteSpace = "\n\r\f\v\t ";
    string::size_type pos = str.find_first_not_of(whiteSpace);
    if (pos == string::npos) {
        str.clear();
    } else {
        str.erase(0, pos);

        pos = str.find_last_not_of(whiteSpace);
        if (pos != string::npos) {
            str.erase(pos + 1);
        }
    }
}

void ipmi_ts_persist_sel_record_id::str_equals_split(const std::string& in_str, std::string& out_left, std::string& out_right) const
{
    size_t pos = in_str.find('=');
    if(string::npos != pos) {
        out_left = in_str.substr(0, pos);
        out_right = in_str.substr(pos + 1);

        str_trim(out_left);
        str_trim(out_right);
    } else {
        out_left = "";
        out_right = "";
    }
}

bool ipmi_ts_persist_sel_record_id::copy_with_replace(std::ifstream& in_strm, std::ofstream& out_strm) const
{
    bool updated = false;
    while(false == in_strm.eof()) {
        string line = get_stream_line(in_strm);
        if(true != line.empty()) {
            string name;
            string val;
            str_equals_split(line, name, val);
            if(name == hostname_) {
                out_strm << hostname_ << "=" << record_id_ << endl;
                updated = true;
            } else if (false == name.empty() && false == val.empty()) {
                out_strm << name << "=" << val << endl;
            }
        }
    }
    if(false == updated) {
        out_strm << hostname_ << "=" << record_id_ << endl;
    }
    return true;
}

bool ipmi_ts_persist_sel_record_id::create_new(const char* filename) const
{
    ofstream strm(filename);
    if(true == strm.fail()) {
        return false;
    }
    strm << hostname_ << "=" << record_id_ << endl;
    strm.close();
    return true;
}
