/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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
#include "orcm/constants.h"
#include "pugixml/pugixml.hpp"

#include "opal/class/opal_list.h"

extern "C" {
#include "orcm/util/utils.h"
}

class pugi_impl {
    public:

        pugi_impl(const std::string& file) { this->file = file; root=OBJ_NEW(opal_list_t); };
        //pugi_impl(char const* file) { this->file = file; root=OBJ_NEW(opal_list_t); };
        pugi_impl() { file = ""; root=OBJ_NEW(opal_list_t); };
        ~pugi_impl() { unloadFile(); freeRoot(); };

        int loadFile();
        opal_list_t* retrieveSection(opal_list_item_t *start,
                                     char const*key, char const*name);

    protected:

        std::string file;
        pugi::xml_document doc;
        opal_list_t *root;

        void unloadFile();
        void freeRoot();
        bool isLeafNode(pugi::xml_node node);
        void addLeafNodeToList(pugi::xml_node node, opal_list_t *list);
        void addNodeAttributesToList(pugi::xml_node node, opal_list_t *list);
        void addNodeChildrenToList(pugi::xml_node node, opal_list_t *list);
        void addNodeToList(pugi::xml_node node, opal_list_t *list);
        int  convertXmlNodeToOpalList(pugi::xml_node node, opal_list_t *list);
        int  extractFromEmptyKeyList(opal_list_t *list);
        bool itemListHasChildren(orcm_value_t *item);
        opal_list_t* searchKeyInList(opal_list_t *srcList, char const *key);
        opal_list_t* searchKeyAndNameInList(opal_list_t *srcList, char const *key, char const* name);
        opal_list_t* searchInList(opal_list_t *list, char const *key,char const* name);
        opal_list_t* duplicateList(opal_list_t *src);
        bool isFullDocumentListRequested(opal_list_item_t *start, char const *key, char const* name);
        bool isKeySearchedFromGivenStart(opal_list_item_t *start, char const *key);
        bool isKeySearchedFromDocumentStart(opal_list_item_t *start, char const *key);
};

#endif
