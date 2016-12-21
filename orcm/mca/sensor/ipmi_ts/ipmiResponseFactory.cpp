/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>

#include <ipmiResponseFactory.hpp>
#include "orcm/util/string_utils.h"

/*Seconds between the epoch (1/1/1970) and IPMI's start time (1/1/1996)*/
#define EPOCH_IPMI_DIFF_TIME 820454400

using namespace std;

IPMIResponse::IPMIResponse(ResponseBuffer* buffer, MessageType type)
{
    if (!buffer || !buffer->size())
        return;

    switch (type)
    {
        case GETDEVICEID_MSG:
            device_id_cmd_to_data_container(buffer);
            break;
        case GETACPIPOWER_MSG:
            acpi_power_cmd_to_data_container(buffer);
            break;
        case GETFRUINVAREA_MSG:
            fru_inv_area_cmd_to_data_container(buffer);
            break;
        case READFRUDATA_MSG:
            fru_data_cmd_to_data_container(buffer);
            break;
        case GETPSUPOWER_MSG:
            psu_power_cmd_to_data_container(buffer);
            break;
        default:
            break;
    }
}

dataContainer IPMIResponse::getDataContainer()
{
    return data_container;
}

void IPMIResponse::device_id_cmd_to_data_container(ResponseBuffer* buffer)
{
    if (GETDEVICEID_SIZE != buffer->size())
        return;

    string tmp_val = hex_to_str((*buffer)[0]);
    add_to_container("device_id", tmp_val);          // byte 2

    tmp_val = hex_to_str((*buffer)[1]);
    add_to_container("device_revision", tmp_val);    // byte 3

    tmp_val = hex_to_str_no_fill((*buffer)[2]) + ".";
    tmp_val += hex_to_str_no_fill((*buffer)[3]);
    add_to_container("bmc_ver", tmp_val);     // byte 4 and 5

    tmp_val = hex_to_str_no_fill((*buffer)[4]&0x0F) +  ".";
    tmp_val += hex_to_str_no_fill((*buffer)[4]&0xF0);
    add_to_container("ipmi_ver", tmp_val);       // byte 6

    tmp_val = hex_to_str((*buffer)[5]);
    add_to_container("additional_support", tmp_val); // byte 7

    tmp_val = hex_to_str_no_fill((*buffer)[8]);
    tmp_val += hex_to_str((*buffer)[7]);
    tmp_val += hex_to_str((*buffer)[6]);
    add_to_container("manufacturer_id", tmp_val);    // byte 8 to 10

    tmp_val = hex_to_str((*buffer)[9]);
    tmp_val += hex_to_str((*buffer)[10]);
    add_to_container("product_id", tmp_val);         // byte 11 and 12

    tmp_val = hex_to_str((*buffer)[11]);
    tmp_val += hex_to_str((*buffer)[12]);
    tmp_val += hex_to_str((*buffer)[13]);
    tmp_val += hex_to_str((*buffer)[14]);
    add_to_container("aux_firmware_info", tmp_val);  // byte 13 to 16
}

void IPMIResponse::psu_power_cmd_to_data_container(ResponseBuffer* buffer)
{
    if (PSUPOWER_SIZE > buffer->size())
        return;

    unsigned char data_a[8] = {0};

    data_a[0]=(*buffer)[1];
    data_a[1]=(*buffer)[2];
    data_a[1]&=0x7f;
    data_a[2]=(*buffer)[3];
    if (data_a[2]&0x1){
        data_a[1]|=0x80;
    }
    data_a[2]>>=1;
    uint64_t reading_a = *((uint64_t*)data_a);

    unsigned char data_b[8] = {0};

    data_b[0]=(*buffer)[4];
    data_b[1]=(*buffer)[5];
    data_b[2]=(*buffer)[6];
    uint64_t reading_b = *((uint64_t*)data_b);

    add_to_container("accu", reading_a);
    add_to_container("cnt", reading_b);
}


void IPMIResponse::acpi_power_cmd_to_data_container(ResponseBuffer* buffer)
{
    if (GETACPIPOWER_SIZE != buffer->size())
        return;

    string tmp_val = get_system_power_state((*buffer)[0] & 0x7F);
    add_to_container("system_power_state", tmp_val); // byte 2

    tmp_val = get_device_power_state((*buffer)[1] & 0x7F);
    add_to_container("device_power_state", tmp_val); // byte 3
}

void IPMIResponse::fru_inv_area_cmd_to_data_container(ResponseBuffer* buffer)
{
    if (GETFRUINVAREA_SIZE != buffer->size())
        return;

    uint32_t tmp_val = (*buffer)[0];
    tmp_val += uint32_t((*buffer)[1]) << 8;
    add_to_container("fru_inv_area_size", tmp_val);              // byte 2 and 3
    add_to_container("device_access_type", (*buffer)[2] & 0x01); // byte 4
}

void IPMIResponse::fru_data_cmd_to_data_container(ResponseBuffer* buffer)
{
    if (READFRUDATA_SIZE >= buffer->size())
        return;

    /*
        Source: Platform Management Fru Document (Rev 1.2)
        Fru data is stored in the following fashion:
        1 Byte - N number of bytes to follow that holds some information
        N Bytes - The information we are after

        So the location of information within buffer is always relative to the
        location of the information that came before it.

        To get to the size of the information to follow, skip past all the
        information you've already read. To the read that information, skip
        past all the information you've already read + 1, then read that number
        of bytes.
    */

    // Board Info Area start offset is stored in [3] as multiples of 8 bytes
    uint32_t fru_offset = (*buffer)[3] * 8;
    get_fru_data(fru_offset, buffer);
}

template<typename T>
void IPMIResponse::add_to_container(string key, T value)
{
    data_container.put(key, value, "");
}

string IPMIResponse::get_system_power_state(uint8_t state)
{
    string str_state = "";
    switch(state)
    {
        case 0x00: str_state="S0/G0"; break;
        case 0x01: str_state="S1"; break;
        case 0x02: str_state="S2"; break;
        case 0x03: str_state="S3"; break;
        case 0x04: str_state="S4"; break;
        case 0x05: str_state="S5/G2"; break;
        case 0x06: str_state="S4/S5"; break;
        case 0x07: str_state="G3"; break;
        case 0x08: str_state="sleeping"; break;
        case 0x09: str_state="G1 sleeping"; break;
        case 0x0A: str_state="S5 override"; break;
        case 0x20: str_state="Legacy On"; break;
        case 0x21: str_state="Legacy Off"; break;
        case 0x2A: str_state="Unknown"; break;
        default:   str_state="Illegal"; break;
    }
    return str_state;
}

string IPMIResponse::get_device_power_state(uint8_t state)
{
    string str_state = "";
    switch(state)
    {
        case 0x00: str_state="D0"; break;
        case 0x01: str_state="D1"; break;
        case 0x02: str_state="D2"; break;
        case 0x03: str_state="D3"; break;
        case 0x04: str_state="Unknown"; break;
        default:   str_state="Illegal"; break;
    }
    return str_state;
}

void IPMIResponse::get_manuf_date(uint64_t fru_offset, ResponseBuffer *buffer)
{
    uint64_t manuf_minutes=0;
    time_t raw_seconds=0;
    struct tm *time_info=0;
    char c_str[11];
    string manuf_date = "";

    fru_offset += 3; // next relevant data starts at position 3 (manuf date)
    manuf_minutes =  (*buffer)[fru_offset++];       // LSByte
    manuf_minutes += (*buffer)[fru_offset++] << 8;
    manuf_minutes += (*buffer)[fru_offset]   << 16; // MSByte

    // IPMI time is stored in minutes from 0:00 1/1/1996 (EPOCH_IPMI_DIFF_TIME)
    // Time from epoch = time from ipmi start + difference from epoch to ipmi start
    raw_seconds = manuf_minutes*60 + EPOCH_IPMI_DIFF_TIME;
    time_info = localtime(&raw_seconds);

    if (NULL != time_info)
    {
        strftime(c_str,10,"%x",time_info);
        manuf_date = string(c_str);
    }

    add_to_container("bb_manufactured_date", manuf_date);
}

void IPMIResponse::get_fru_data(uint64_t fru_offset, ResponseBuffer *buffer)
{
    vector<string> fru_data;
    fru_data.push_back("bb_vendor");
    fru_data.push_back("bb_product");
    fru_data.push_back("bb_serial");
    fru_data.push_back("bb_part");

    uint8_t board_data_len = 0;
    string board_data = "";

    get_manuf_date(fru_offset, buffer);

    fru_offset += 6; // next relevant data starts at position 6 (manuf name)
    for (vector<string>::iterator it=fru_data.begin(); it!=fru_data.end(); ++it)
    {
        board_data = "";
        board_data_len = (*buffer)[fru_offset] & 0x3f;
        fru_offset++; // incrementing the length byte

        for(int i=0; i<board_data_len; i++)
            board_data += (*buffer)[fru_offset + i];
        board_data += '\0';
        fru_offset += board_data_len; // adding last data len

        add_to_container(*it, board_data);
    }
}
