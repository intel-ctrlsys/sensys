/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef XMLMANAGER_HPP
#define XMLMANAGER_HPP
#include "parsers/pugixml/pugixml.hpp"
#include <iostream>
using std::string;

class xmlManager {
public:
    xmlManager();
    int open_file(string);
    pugi::xml_node* get_root_node(string);
    void print_node(pugi::xml_node, string);
private:
    pugi::xml_document document;
};
#endif /* XMLMANAGER_HPP */
