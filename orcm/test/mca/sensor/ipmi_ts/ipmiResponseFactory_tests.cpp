/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <vector>

#include "orcm/common/dataContainer.hpp"

#include "ipmiResponseFactory_tests.hpp"

using namespace std;

TEST_F(IPMIResponseTests, null_buffer)
{
    IPMIResponse response(NULL, GETDEVICEID_MSG);
    dataContainer dc = response.getDataContainer();
    ASSERT_FALSE(dc.count());
}

TEST_F(IPMIResponseTests, empty_buffer)
{
    ResponseBuffer buffer;
    IPMIResponse response(&buffer, GETDEVICEID_MSG);
    dataContainer dc = response.getDataContainer();
    ASSERT_FALSE(dc.count());
}

TEST_F(IPMIResponseTests, unhandled_type)
{
    ResponseBuffer buffer;
    buffer.push_back(0x02); // Device ID
    IPMIResponse response(&buffer, GETSENSORLIST_MSG);
    dataContainer dc = response.getDataContainer();
    ASSERT_FALSE(dc.count());
}

TEST_F(IPMIResponseTests, get_device_id_wrong_size)
{
    test_wrong_size(GETDEVICEID_MSG);
}

TEST_F(IPMIResponseTests, get_acpi_power_wrong_size)
{
    test_wrong_size(GETACPIPOWER_MSG);
}

TEST_F(IPMIResponseTests, get_fru_inv_area_wrong_size)
{
    test_wrong_size(GETFRUINVAREA_MSG);
}

TEST_F(IPMIResponseTests, read_fru_data_wrong_size)
{
    test_wrong_size(READFRUDATA_MSG);
}

TEST_F(IPMIResponseTests, get_device_id_cmd)
{
    ResponseBuffer buffer;
    buffer.push_back(0x02); // Device ID
    buffer.push_back(0x03); // Device Revision
    buffer.push_back(0x04); // Firmware Revision 1
    buffer.push_back(0x05); // Firmware Revision 2
    buffer.push_back(0x06); // IPMI Version
    buffer.push_back(0x07); // Additional Device Support
    buffer.push_back(0x08); // Manufacturer ID (3 bytes)
    buffer.push_back(0x09);
    buffer.push_back(0x0A);
    buffer.push_back(0x0B); // Product ID (2 bytes)
    buffer.push_back(0x0C);
    buffer.push_back(0x0D); // Auxiliary Firmware Revision Info (4 bytes)
    buffer.push_back(0x0E);
    buffer.push_back(0x0F);
    buffer.push_back(0x10);

    IPMIResponse response(&buffer, GETDEVICEID_MSG);
    dataContainer dc = response.getDataContainer();

    string tmp_val = dc.getValue<string>("device_id");
    ASSERT_FALSE(tmp_val.compare("02"));

    tmp_val = dc.getValue<string>("device_revision");
    ASSERT_FALSE(tmp_val.compare("03"));

    tmp_val = dc.getValue<string>("bmc_ver");
    ASSERT_FALSE(tmp_val.compare("4.5"));

    tmp_val = dc.getValue<string>("ipmi_ver");
    ASSERT_FALSE(tmp_val.compare("6.0"));

    tmp_val = dc.getValue<string>("additional_support");
    ASSERT_FALSE(tmp_val.compare("07"));

    tmp_val = dc.getValue<string>("manufacturer_id");
    ASSERT_FALSE(tmp_val.compare("a0908"));

    tmp_val = dc.getValue<string>("product_id");
    ASSERT_FALSE(tmp_val.compare("0b0c"));

    tmp_val = dc.getValue<string>("aux_firmware_info");
    ASSERT_FALSE(tmp_val.compare("0d0e0f10"));
}

TEST_F(IPMIResponseTests, acpi_power_cmd_first_bit_ignored)
{
    ResponseBuffer buffer;

    buffer.push_back(0x80); // System power state (first bit is ignored)
    buffer.push_back(0x80); // Device power state (first bit is ignored)

    IPMIResponse response(&buffer, GETACPIPOWER_MSG);
    dataContainer dc = response.getDataContainer();

    string tmp_val = dc.getValue<string>("system_power_state");
    ASSERT_FALSE(tmp_val.compare("S0/G0"));

    tmp_val = dc.getValue<string>("device_power_state");
    ASSERT_FALSE(tmp_val.compare("D0"));
}

TEST_F(IPMIResponseTests, acpi_power_cmd_all_combinations)
{
    ResponseBuffer buffer;

    for (PowerMap::iterator it=sys_pwr.begin(); it!=sys_pwr.end(); ++it)
    {
        buffer.push_back(it->first); // System power state
        buffer.push_back(it->first); // Device power state

        cout << "Value: " << hex << int(it->first);

        IPMIResponse response(&buffer, GETACPIPOWER_MSG);
        dataContainer dc = response.getDataContainer();

        string tmp_val = dc.getValue<string>("system_power_state");
        ASSERT_FALSE(tmp_val.compare(it->second));
        cout << ",\t System power: " << tmp_val;

        tmp_val = dc.getValue<string>("device_power_state");
        if (dev_pwr.find(it->first) == dev_pwr.end())
            ASSERT_FALSE(tmp_val.compare(dev_pwr[0xFF]));
        else
            ASSERT_FALSE(tmp_val.compare(dev_pwr[it->first]));
        cout << ",\t Device power: " << tmp_val << endl;

        buffer.clear();
    }
}

TEST_F(IPMIResponseTests, get_fru_inv_area_cmd)
{
    ResponseBuffer buffer;
    buffer.push_back(0x02); // FRU inventory area size LS byte
    buffer.push_back(0x03); // FRU inventory area size MS byte
    buffer.push_back(0x0F); // Device Access type

    IPMIResponse response(&buffer, GETFRUINVAREA_MSG);
    dataContainer dc = response.getDataContainer();

    ASSERT_EQ(0x0302, dc.getValue<uint16_t>("fru_inv_area_size"));
    ASSERT_EQ(0x01, dc.getValue<uint8_t>("device_access_type"));
}

TEST_F(IPMIResponseTests, read_fru_data_cmd_empty_fru_data)
{
    ResponseBuffer buffer;
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x01); // Board starting offset, multiples of 8 bytes
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Board Area Format Version (not needed)
    buffer.push_back(0x00); // Board Area Length (not needed)
    buffer.push_back(0x00); // Language code (not needed)
    buffer.push_back(0x88); // Manuf date (3 bytes)
    buffer.push_back(0xCF);
    buffer.push_back(0xA7);
    buffer.push_back(0x00); // Manuf name length
    buffer.push_back(0x04); // Product name length
    buffer.push_back('N');  // Product name
    buffer.push_back('A');
    buffer.push_back('M');
    buffer.push_back('E');
    buffer.push_back(0x00); // Serial number length
    buffer.push_back(0x00); // Part number length
    buffer.push_back(0xA1); // Garbage
    buffer.push_back(0xA2); // Garbage
    buffer.push_back(0xA3); // Garbage

    IPMIResponse response(&buffer, READFRUDATA_MSG);
    dataContainer dc = response.getDataContainer();

    string tmp_val = dc.getValue<string>("bb_manufactured_date");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("11/28/16"));

    tmp_val = dc.getValue<string>("bb_vendor");
    ASSERT_FALSE(tmp_val.compare(""));

    tmp_val = dc.getValue<string>("bb_product");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("NAME"));

    tmp_val = dc.getValue<string>("bb_serial");
    ASSERT_FALSE(tmp_val.compare(""));

    tmp_val = dc.getValue<string>("bb_part");
    ASSERT_FALSE(tmp_val.compare(""));
}
void fill_fru_data_buffer(ResponseBuffer& buffer)
{
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x01); // Board starting offset, multiples of 8 bytes
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00); // Board Area Format Version (not needed)
    buffer.push_back(0x00); // Board Area Length (not needed)
    buffer.push_back(0x00); // Language code (not needed)
    buffer.push_back(0x88); // Manuf date (3 bytes)
    buffer.push_back(0xCF);
    buffer.push_back(0xA7);
    buffer.push_back(0x05); // Manuf name length
    buffer.push_back('M');  // Manuf name
    buffer.push_back('A');
    buffer.push_back('N');
    buffer.push_back('U');
    buffer.push_back('F');
    buffer.push_back(0x04); // Product name length
    buffer.push_back('N');  // Product name
    buffer.push_back('A');
    buffer.push_back('M');
    buffer.push_back('E');
    buffer.push_back(0x06); // Serial number length
    buffer.push_back('1');  // Serial number
    buffer.push_back('2');
    buffer.push_back('3');
    buffer.push_back('4');
    buffer.push_back('5');
    buffer.push_back('6');
    buffer.push_back(0x02); // Part number length
    buffer.push_back('4');
    buffer.push_back('2');
    buffer.push_back(0xA1); // Garbage
    buffer.push_back(0xA2); // Garbage
    buffer.push_back(0xA3); // Garbage
}

TEST_F(IPMIResponseTests, read_fru_data_cmd)
{
    ResponseBuffer buffer;
    fill_fru_data_buffer(buffer);
    IPMIResponse response(&buffer, READFRUDATA_MSG);
    dataContainer dc = response.getDataContainer();

    string tmp_val = dc.getValue<string>("bb_manufactured_date");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("11/28/16"));

    tmp_val = dc.getValue<string>("bb_vendor");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("MANUF"));

    tmp_val = dc.getValue<string>("bb_product");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("NAME"));

    tmp_val = dc.getValue<string>("bb_serial");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("123456"));

    tmp_val = dc.getValue<string>("bb_part");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("42"));
}


TEST_F(IPMIResponseTests, read_fru_data_cmd_empty_manuf_date)
{
    ResponseBuffer buffer;
    fill_fru_data_buffer(buffer);
    mocks[LOCALTIME].pushState(FAILURE);
    IPMIResponse response(&buffer, READFRUDATA_MSG);
    dataContainer dc = response.getDataContainer();

    string tmp_val = dc.getValue<string>("bb_manufactured_date");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare(""));

    tmp_val = dc.getValue<string>("bb_vendor");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("MANUF"));

    tmp_val = dc.getValue<string>("bb_product");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("NAME"));

    tmp_val = dc.getValue<string>("bb_serial");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("123456"));

    tmp_val = dc.getValue<string>("bb_part");
    cout << tmp_val << endl;
    ASSERT_FALSE(tmp_val.compare("42"));
}

void IPMIResponseTests::test_wrong_size(MessageType msg)
{
    ResponseBuffer buffer;
    buffer.push_back(0x00); // Garbage

    IPMIResponse response(&buffer, msg);
    dataContainer dc = response.getDataContainer();

    ASSERT_EQ(0, dc.count());
}
