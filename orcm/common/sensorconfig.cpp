/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "parsers/pugixml/pugixml.hpp"
#include "xmlManager.hpp"
#include "sensorconfig.hpp"
#include "sensorConfigNodeXml.hpp"
#include <string>
#include <iostream>
using std::string;

sensorConfigNodeInterface::sensorConfigNodeInterface(void){};

sensorConfigNodeInterface::~sensorConfigNodeInterface(void){};

sensorConfigNodePointer build_xml_root_node(string, string);
xmlManager document_manager;
extern "C" sensorConfigNodePointer
getConfigFromFile(string file_path, File_types file_type, string tag) {
    sensorConfigNodePointer result_node;
    switch (file_type) {
        case XML:
            result_node = build_xml_root_node(file_path, tag);
            break;
        default:
            std::cerr << tag << ": "
                    << "Unknown [" << file_type << "] file type" << std::endl;
            result_node = NULL;
            break;
    }
    return result_node;
}

sensorConfigNodePointer build_xml_root_node(string file_path, string tag) {
    if (0 == document_manager.open_file(file_path)) {

        pugi::xml_node* sensors_node =
                document_manager.get_root_node(tag);

        if (NULL != sensors_node) {
            sensorConfigNodePointer return_value;
            return_value = sensorConfigNodePointer(new sensorConfigNodeXml(*sensors_node));
            delete sensors_node;
            return return_value;
        }
    }
    return NULL;
}
