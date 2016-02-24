/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef XML_PARSER_H
#define XML_PARSER_H

#include "orcm/constants.h"
#include "pugixml/pugixml.hpp"
#include "parser.h"
#include "parser_utils.h"
#include <string.h>

class xmlparser: public parser {
    public:
        xmlparser(std::string file) : parser(file) {};
        xmlparser(char const* file="") : parser(file) {};
        ~xmlparser();

        int loadFile();
        int getValue(char** ptrToValue, char const* key);
        int getArray(char*** ptrToArray, int* size, char const* key);

    private:
        pugi::xml_document doc;
        void unloadFile();
        bool isKeyWellFormed(char const* key);
        pugi::xml_node findNode(char const* key);
        pugi::xml_node getNodeWithoutName(pugi::xml_node node,
                                          std::string tag_name);
        pugi::xml_node getNodeWithName(pugi::xml_node node,
                                       std::string tag_name, std::string name);
        char * getValueFromNode(pugi::xml_node node);
        std::vector< std::vector<std::string> > splitKey(char const* key);
        bool keyEndsWithName(char const *key);
};

#endif // XML_PARSER_H

