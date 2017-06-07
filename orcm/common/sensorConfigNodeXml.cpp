/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sensorConfigNodeXml.hpp"

using std::string;

bool sensorConfigNodeXml::node_is_attribute(pugi::xml_node node) {
    return !string(node.value()).empty();
}

string sensorConfigNodeXml::get_attribute(string name) {
    string return_value = source_node.child_value(name.c_str());
    if (return_value.empty()) {
        return_value = source_node.attribute(name.c_str()).value();
    }
    return return_value;
}

attributes_list_t sensorConfigNodeXml::get_attributes_list(void) {
    attributes_list_t result_list;
    std::pair<string, string> item_list;

    for (auto it = source_node.attributes_begin();
         it != source_node.attributes_end();
         it++) {

        item_list.first = it->name();
        item_list.second = it->value();
        result_list.push_back(item_list);
    }

    for (auto it = source_node.begin(); it != source_node.end(); it++) {
        item_list.first = it->name();
        item_list.second = it->value();
        if (node_is_attribute(*it))
            result_list.push_back(item_list);
    }

    return result_list;
}

string_list_t sensorConfigNodeXml::get_attributes_list(string tag) {
    string_list_t result_list;
    string item_value = source_node.attribute(tag.c_str()).value();

    if (!item_value.empty())
        result_list.push_back(item_value);

    for (auto candidate = source_node.child(tag.c_str());
         candidate;
         candidate = candidate.next_sibling(tag.c_str())) {

        if (node_is_attribute(candidate))
            result_list.push_back(string(candidate.value()));
    }

    return result_list;
}

objects_list_t sensorConfigNodeXml::get_object_list(void) {
    objects_list_t result_list;
    for (auto it = source_node.begin(); it != source_node.end(); it++) {
        if (!node_is_attribute(*it))
            result_list.push_back(sensorConfigNodePointer(new sensorConfigNodeXml(*it)));
    }

    return result_list;
}

objects_list_t sensorConfigNodeXml::get_object_list(string tag) {
    objects_list_t result_list;

    for (auto candidate = source_node.child(tag.c_str());
         candidate;
         candidate = candidate.next_sibling(tag.c_str())) {

        if (!node_is_attribute(candidate))
            result_list.push_back(sensorConfigNodePointer(new sensorConfigNodeXml(candidate)));
    }

    return result_list;
}

objects_list_t
sensorConfigNodeXml::get_object_list_by_attribute(string tag,
                                                    string attribute,
                                                    string value) {

    objects_list_t result_list;
    string candidate_value;

    for (auto candidate = source_node.child(tag.c_str());
         candidate;
         candidate = candidate.next_sibling(tag.c_str())) {

        candidate_value = candidate.attribute(attribute.c_str()).value();

        if (candidate_value == value)
            result_list.push_back(sensorConfigNodePointer(new sensorConfigNodeXml(candidate)));
        else {
            for (auto child = candidate.child(attribute.c_str());
                 child;
                 child = child.next_sibling(attribute.c_str())) {

                if (value == child.value())
                    result_list.push_back(sensorConfigNodePointer(new sensorConfigNodeXml(candidate)));
            }
        }
    }

    return result_list;
}

string sensorConfigNodeXml::get_tag(void) {
    return source_node.name();
}
