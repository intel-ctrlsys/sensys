/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIRESPONSEFACTORY_H
#define IPMIRESPONSEFACTORY_H

#include <vector>

#include "orcm/common/dataContainer.hpp"

#define GETDEVICEID_SIZE 16
#define GETACPIPOWER_SIZE 3
#define GETFRUINVAREA_SIZE 4
#define READFRUDATA_SIZE 4

using namespace std;
enum MessageType
{
    GETDEVICEID = 0,
    GETACPIPOWER,
    GETFRUINVAREA,
    READFRUDATA
};

typedef vector<uint8_t> ResponseBuffer;

class IPMIResponse
{
    public:
        IPMIResponse(ResponseBuffer*, MessageType);
        dataContainer getDataContainer(void);
    private:
        dataContainer data_container;
        void device_id_cmd_to_data_container(ResponseBuffer*);
        void acpi_power_cmd_to_data_container(ResponseBuffer*);
        void fru_inv_area_cmd_to_data_container(ResponseBuffer*);
        void fru_data_cmd_to_data_container(ResponseBuffer*);
        string get_system_power_state(uint8_t);
        string get_device_power_state(uint8_t);
        void get_manuf_date(uint64_t, ResponseBuffer*);
        void get_fru_data(uint64_t, ResponseBuffer*);
        template<typename T> void add_to_container(string, T);
};

#endif // IPMIRESPONSEFACTORY_H