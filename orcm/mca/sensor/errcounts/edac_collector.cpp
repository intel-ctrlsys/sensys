/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "edac_collector.h"

extern "C" {
    #include <errno.h>
    #include <string.h>
}

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace std;

edac_collector::edac_collector(edac_error_callback_fn_t error_cb, const char* edac_path) :
    error_callback(error_cb), user_data_(0), previous_sample_(NULL)
{
    if(NULL != edac_path) {
        base_edac_path = string(edac_path);
    } else {
        base_edac_path = string("/sys/devices/system/edac/mc");
    }
    previous_sample_ = new SampleMap;
}

edac_collector::~edac_collector()
{
    delete previous_sample_;
}

FILE* edac_collector::FOpen(const char* path, const char* mode) const
{
    return fopen(path, mode);
}

ssize_t edac_collector::GetLine(char** lineptr, size_t* n, FILE* stream) const
{
    return getline(lineptr, n, stream);
}

int edac_collector::FClose(FILE* fd) const
{
    return fclose(fd);
}

int edac_collector::Stat(const char* path, struct stat* info) const
{
    return stat(path, info);
}

bool edac_collector::collect_data(edac_data_callback_fn_t cb, void* user_data)
{
    if(NULL == cb) {
        return false;
    }
    user_data_ = (unsigned long long)user_data;
    int mc_count = get_mc_folder_count();
    for(int mc = 0; mc < mc_count; ++mc) {
        int csrow_count = get_csrow_folder_count(mc);
        for(int csrow = 0; csrow < csrow_count; ++csrow) {
            int ce_count = get_ce_count(mc, csrow);
            int ue_count = get_ue_count(mc, csrow);
            if(-1 == ce_count || -1 == ue_count) {
                continue;
            }
            stringstream ss;
            ss << "CPU_SrcID#" << mc << "_Sum_DIMM#" << csrow << "_CE";
            log_data(ss.str().c_str(), ce_count, cb);
            ss.str("");
            ss << "CPU_SrcID#" << mc << "_Sum_DIMM#" << csrow << "_UE";
            log_data(ss.str().c_str(), ue_count, cb);

            int channel_count = get_channel_folder_count(mc, csrow);
            for(int channel = 0; channel < channel_count; ++channel) {
                string label = get_channel_label(mc, csrow, channel) + "_CE";
                ce_count = get_channel_ce_count(mc, csrow, channel);
                if("" != label && -1 != ce_count) {
                    log_data(label.c_str(), ce_count, cb);
                }
            }
        }
    }
    return true;
}

bool edac_collector::collect_inventory(edac_inventory_callback_fn_t cb, void* user_data)
{
    if(NULL == cb) {
        return false;
    }
    user_data_ = (unsigned long long)user_data;
    int sensor_count = 0;
    int mc_count = get_mc_folder_count();
    for(int mc = 0; mc < mc_count; ++mc) {
        int csrow_count = get_csrow_folder_count(mc);
        for(int csrow = 0; csrow < csrow_count; ++csrow) {
            stringstream ss;
            string label;
            string name;
            ss << "CPU_SrcID#" << mc << "_Sum_DIMM#" << csrow << "_CE";
            name = ss.str();
            ss.str("");
            ss << "sensor_errcounts_" << ++sensor_count;
            label = ss.str();
            ss.str("");
            log_inventory(label.c_str(), name.c_str(), cb);

            ss << "CPU_SrcID#" << mc << "_Sum_DIMM#" << csrow << "_UE";
            name = ss.str();
            ss.str("");
            ss << "sensor_errcounts_" << ++sensor_count;
            label = ss.str();
            ss.str("");
            log_inventory(label.c_str(), name.c_str(), cb);

            int channel_count = get_channel_folder_count(mc, csrow);
            for(int channel = 0; channel < channel_count; ++channel) {
                name = get_channel_label(mc, csrow, channel) + "_CE";
                ss << "sensor_errcounts_" << ++sensor_count;
                label = ss.str();
                ss.str("");
                log_inventory(label.c_str(), name.c_str(), cb);
            }
        }
    }
    return true;
}

bool edac_collector::have_edac() const
{
    struct stat sb;
    if (0 == Stat(base_edac_path.c_str(), &sb) && S_ISDIR(sb.st_mode)) {
        return true;
    }
    return false;
}

int edac_collector::get_mc_folder_count() const
{
    int count = 0;
    stringstream ss;
    struct stat sb;
    for(int i = 0; ; ++i) {
        ss << base_edac_path << "/mc" << i;
        if(0 == Stat(ss.str().c_str(), &sb)) {
            ++count;
            ss.str("");
        } else {
            break;
        }
    }
    return count;
}

int edac_collector::get_csrow_folder_count(int mc) const
{
    int count = 0;
    stringstream ss;
    struct stat sb;
    for(int i = 0; ; ++i) {
        ss << base_edac_path << "/mc" << mc << "/csrow" << i;
        if(0 == Stat(ss.str().c_str(), &sb)) {
            ++count;
            ss.str("");
        } else {
            break;
        }
    }
    return count;
}

int edac_collector::get_channel_folder_count(int mc, int csrow) const
{
    int count = 0;
    stringstream ss;
    struct stat sb;
    for(int i = 0; ; ++i) {
        ss << base_edac_path << "/mc" << mc << "/csrow" << csrow << "/ch" << i << "_dimm_label";
        if(0 == Stat(ss.str().c_str(), &sb)) {
            ++count;
            ss.str("");
        } else {
            break;
        }
    }
    return count;
}

void edac_collector::log_data(const char* label, int count, edac_data_callback_fn_t cb) const
{
    int old_sample = -1;
    string l(label);
    if(previous_sample_->end() != previous_sample_->find(l)) {
        old_sample = (*previous_sample_)[l];
    }
    if(count != old_sample || -1 == old_sample) {
        (*previous_sample_)[l] = count;
        cb(label, count, (void*)user_data_);
    }
}

void edac_collector::log_inventory(const char* label, const char* name, edac_inventory_callback_fn_t cb) const
{
    cb(label, name, (void*)user_data_);
}

void edac_collector::report_error(const char* pathname, int error_number) const
{
    if(NULL != error_callback) {
        error_callback(pathname, error_number, (void*)user_data_);
    }
}

int edac_collector::get_xx_count(int mc, int csrow, const char* filename) const
{
    stringstream ss;
    ss << base_edac_path << "/mc" << mc << "/csrow" << csrow << "/" << filename;
    FILE* fd = FOpen(ss.str().c_str(), "r");
    if(NULL == fd) {
        report_error(ss.str().c_str(), errno);
        return -1;
    }
    char* buffer = NULL;
    size_t buffer_size = 0;
    if(-1 == GetLine(&buffer, &buffer_size, fd)) {
        report_error(ss.str().c_str(), errno);
        FClose(fd);
        return -1;
    }
    FClose(fd);
    int count = atoi(buffer);
    free(buffer);
    return count;
}

int edac_collector::get_ue_count(int mc, int csrow) const
{
    return get_xx_count(mc, csrow, "ue_count");
}

int edac_collector::get_ce_count(int mc, int csrow) const
{
    return get_xx_count(mc, csrow, "ce_count");
}

int edac_collector::get_channel_ce_count(int mc, int csrow, int channel) const
{
    stringstream ss;
    ss << "ch" << channel << "_ce_count";
    return get_xx_count(mc, csrow, ss.str().c_str());
}

std::string edac_collector::get_channel_label(int mc, int csrow, int channel) const
{
    stringstream ss;
    ss << base_edac_path << "/mc" << mc << "/csrow" << csrow << "/ch" << channel << "_dimm_label";
    FILE* fd = FOpen(ss.str().c_str(), "r");
    if(NULL == fd) {
        report_error(ss.str().c_str(), errno);
        return string();
    }
    char* buffer = NULL;
    size_t buffer_size = 0;
    if(-1 == GetLine(&buffer, &buffer_size, fd)) {
        report_error(ss.str().c_str(), errno);
        FClose(fd);
        return string();
    }
    FClose(fd);
    string rv = buffer;
    free(buffer);
    remove_newlines(rv);
    return rv;
}

std::string& edac_collector::remove_newlines(std::string& str) const
{
    size_t pos;
    while(string::npos != (pos = str.find('\n'))) {
        str.erase(pos, 1);
    }
    return str;
}
