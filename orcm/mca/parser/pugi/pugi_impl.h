/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PUGI_IMPL_H
#define PUGI_IMPL_H

#include <stdlib.h>
#include <string>
#include <stdexcept>
#include "orcm/constants.h"
#include "pugixml/pugixml.hpp"

#include "opal/class/opal_list.h"

extern "C" {
#include "orcm/util/utils.h"
}

class pugi_impl {
    public:

        pugi_impl(const std::string& file) { this->file = file; root=OBJ_NEW(opal_list_t);
                                             root_element = "configuration";};
        //pugi_impl(char const* file) { this->file = file; root=OBJ_NEW(opal_list_t); };
        pugi_impl() { file = ""; root=OBJ_NEW(opal_list_t); root_element = "configuration";};
        ~pugi_impl() { unloadFile(); freeRoot(); };

        int loadFile();
        opal_list_t* retrieveSection(char const*key, char const*name);
        opal_list_t* retrieveSectionFromList(opal_list_item_t *start,
                                     char const*key, char const*name);
        opal_list_t* retrieveDocument();
        int writeSection(opal_list_t *input, char const*key, char const*name, bool overwrite);

    protected:

        std::string file;
        pugi::xml_document doc;
        opal_list_t *root;
        std::string root_element;

        void unloadFile();
        void freeRoot();
        int  saveSection();

        int  convertOpalListToXmlNodes(opal_list_t *list, pugi::xml_node& key_node);
        int  convertOpalPtrToXmlNodes(pugi::xml_node& key_node, orcm_value_t *list_item);
        int  createNodeFromList(orcm_value_t *list_item, pugi::xml_node& key_node);
        int  writeToTree(opal_list_t *srcList, opal_list_t *input, char const* key,
                         char const*name, bool overwrite);
        int  checkOpalPtrToWrite(orcm_value_t *item, opal_list_t *input, char const* key,
                                 char const* name, bool overwrite);
        void appendToList(opal_list_t **srcList, opal_list_t *input, bool overwrite);
        int appendListToRootNode(opal_list_t *srcList, opal_list_t *input, char const* key,
                                 char const* name);
        void addLeafNodeToList(pugi::xml_node node, opal_list_t *list);
        void addNodeAttributesToList(pugi::xml_node node, opal_list_t *list);
        void addNodeChildrenToList(pugi::xml_node node, opal_list_t *list);
        void addNodeToList(pugi::xml_node node, opal_list_t *list);
        void addValuesToList(opal_list_t *list, char const *key, char const* value);
        int  convertXmlNodeToOpalList(pugi::xml_node node, opal_list_t *list);
        int  extractFromEmptyKeyList(opal_list_t *list);
        bool itemListHasChildren(orcm_value_t *item);
        bool isLeafNode(pugi::xml_node node);
        bool isCommentNode(pugi::xml_node node);
        void addCommentNodeToList(pugi::xml_node node, opal_list_t *list);
        bool itemMatchesKeyAndName(orcm_value_t *item, char const *key, char const* name);
        void joinLists(opal_list_t **list, opal_list_t **otherList);
        opal_list_t* duplicateList(opal_list_t *src);
        opal_list_t* searchKeyInList(opal_list_t *srcList, char const *key);
        opal_list_t* searchKeyAndNameInList(opal_list_t *srcList, char const *key, char const* name);
        opal_list_t* searchInList(opal_list_t *list, char const *key,char const* name);
        opal_list_t* searchKeyInTree(opal_list_t *tree, char const *key);
        opal_list_t* searchKeyAndNameInTree(opal_list_t *tree, char const *key, char const* name);
        opal_list_t* searchInTree(opal_list_t *tree, char const *key,char const* name);
};

class unableToOpenFile : public std::runtime_error {
    public:
    unableToOpenFile(const std::string& file, const std::string& pugi_error_msg) :
        std::runtime_error("Cannot open file '" + file + "' because " + pugi_error_msg) {}
};

#endif
