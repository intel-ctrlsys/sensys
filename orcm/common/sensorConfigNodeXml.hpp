/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSORCONFIGNODEXML_HPP
#define SENSORCONFIGNODEXML_HPP
#include "parsers/pugixml/pugixml.hpp"
#include "sensorconfig.hpp"
#include <list>
using std::string;

class sensorConfigNodeXml : public sensorConfigNodeInterface {
private:
    pugi::xml_node source_node;
    bool node_is_attribute(pugi::xml_node node);
public:

    sensorConfigNodeXml(pugi::xml_node node) {
        this->source_node = node;
    };

    string get_attribute(string);
    attributes_list_t get_attributes_list(void);
    string_list_t get_attributes_list(string);
    objects_list_t get_object_list(void);
    objects_list_t get_object_list(string);
    objects_list_t get_object_list_by_attribute(string, string, string);
    string get_tag(void);
};
#endif /* SENSORCONFIGNODEXML_HPP */

