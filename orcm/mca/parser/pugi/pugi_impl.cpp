/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "pugi_impl.h"
#include "orcm/util/string_utils.h"

using namespace pugi;
using namespace std;


int pugi_impl::loadFile()
{
    xml_parse_result result = doc.load_file(file.c_str());
    if (result){
        return convertXmlNodeToOpalList(doc,root);
    }
    throw unableToOpenFile(file, result.description());
}

opal_list_t* pugi_impl::retrieveSection(char const *key, char const* name)
{
    if (NULL == key || 0 == strcmp("",key)){
        return NULL;
    }
    return searchInTree(root,key,name);
}

opal_list_t* pugi_impl::retrieveSectionFromList(opal_list_item_t *start, char const *key,
                                        char const* name)
{
    if (NULL != start && NULL != key && 0 != strcmp("",key)){
        orcm_value_t *s = (orcm_value_t *)start;
        if (itemListHasChildren(s)){
            return searchInList((opal_list_t*)(s->value.data.ptr), key,name);
        }
    }
    return NULL;
}

opal_list_t* pugi_impl::retrieveDocument(void)
{
    return duplicateList(root);
}

int pugi_impl::writeSection(opal_list_t *input, char const*key,
                            char const* name, bool overwrite)
{
    int rc;

    rc = writeToTree(root, input, key, name, overwrite);

    if (ORCM_ERR_NOT_FOUND == rc) {
        rc = appendListToRootNode(root, input, key, name);
        if (rc != ORCM_SUCCESS) {
            SAFE_RELEASE_NESTED_LIST(input);
            return rc;
        }
        return saveSection();
    }
    else if (ORCM_SUCCESS != rc) {
        SAFE_RELEASE_NESTED_LIST(input);
        return rc;
    }

    return saveSection();
}

int pugi_impl::saveSection()
{
    unloadFile();

    int rc = convertOpalListToXmlNodes(root, doc);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    if (0 == doc.save_file(file.c_str())) {
        return ORCM_ERR_FILE_WRITE_FAILURE;
    }
    return ORCM_SUCCESS;
}

int pugi_impl::convertOpalListToXmlNodes(opal_list_t *list, xml_node& key_node)
{
    orcm_value_t *list_item = NULL;
    int rc;

    if (NULL == list) {
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(list_item, list, orcm_value_t) {
        if (list_item->value.type == OPAL_STRING) {
           rc = createNodeFromList(list_item, key_node);
            if (ORCM_SUCCESS != rc) {
                return rc;
            }
        } else if (list_item->value.type == OPAL_PTR) {
            rc = convertOpalPtrToXmlNodes(key_node, list_item);
            if (ORCM_SUCCESS != rc) {
                return rc;
            }
        } else if (list_item->value.type == OPAL_INT) {
            return ORCM_ERR_NOT_IMPLEMENTED;
        }
    }
    return ORCM_SUCCESS;
}

int pugi_impl::convertOpalPtrToXmlNodes(xml_node& key_node, orcm_value_t *list_item)
{
    xml_node node = key_node.append_child();
    if (NULL == node) {
        return ORCM_ERROR;
    }
    node.set_name(list_item->value.key);
    if (NULL != list_item->value.data.ptr) {
        return convertOpalListToXmlNodes((opal_list_t*)list_item->value.data.ptr, node);
    }
    else {
        return ORCM_SUCCESS;
    }
}

int pugi_impl::createNodeFromList(orcm_value_t *list_item, xml_node& key_node)
{
    xml_node node;

    if (0 == strcmp("name", list_item->value.key)) {
        key_node.append_attribute(list_item->value.key) = list_item->value.data.string;
    }
    else {
        node = key_node.append_child();
        if (NULL == node) {
            return ORCM_ERROR;
        }
        node.set_name(list_item->value.key);
        node.append_child(node_pcdata).set_value(list_item->value.data.string);
    }
    return ORCM_SUCCESS;
}

int pugi_impl::writeToTree(opal_list_t *srcList, opal_list_t *input, char const* key,
                           char const* name, bool overwrite)
{
    int rc;

    if (NULL == srcList){
        return ORCM_ERR_BAD_PARAM;
    }

    if ((NULL == key) || (0 == strcmp("", key))) {
        appendToList(&srcList, input, overwrite);
        return ORCM_SUCCESS;
    }
    orcm_value_t *item = NULL;
    OPAL_LIST_FOREACH(item, srcList, orcm_value_t){

        if (true == itemListHasChildren(item)) {
            rc = checkOpalPtrToWrite(item, input, key, name, overwrite);
            if (ORCM_SUCCESS == rc) {
                return rc;
            }
            rc = writeToTree((opal_list_t*)item->value.data.ptr, input, key,
                                 name, overwrite);
            if (ORCM_SUCCESS == rc) {
                return rc;
            }
        }
    }
    return ORCM_ERR_NOT_FOUND;
}

int pugi_impl::appendListToRootNode(opal_list_t *srcList, opal_list_t *input, char const* key,
                                    char const* name)
{
    opal_list_t *modified_input = OBJ_NEW(opal_list_t);
    orcm_value_t *list_first_element = NULL;
    int rc;

    if (NULL != name && (0 != strcmp("",name))) {
        addValuesToList(input, "name", name);
    }

    char *keyPtr = strdup(key);
    if (NULL == keyPtr) {
        SAFE_RELEASE_NESTED_LIST(modified_input);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    rc = orcm_util_append_orcm_value (modified_input, keyPtr, input, OPAL_PTR, NULL);
    if (rc != ORCM_SUCCESS) {
        SAFEFREE(keyPtr);
        SAFE_RELEASE(modified_input);
        return rc;
    }

    list_first_element = (orcm_value_t *)opal_list_get_first(srcList);
    if (OPAL_PTR != list_first_element->value.type) {
        SAFE_RELEASE(modified_input);
        SAFEFREE(keyPtr);
        return ORCM_ERROR;
    }

    appendToList((opal_list_t**)&(list_first_element->value.data.ptr), modified_input, false);
    SAFEFREE(keyPtr);
    return ORCM_SUCCESS;
}

int pugi_impl::checkOpalPtrToWrite(orcm_value_t *item, opal_list_t *input, char const* key,
                                   char const* name, bool overwrite)
{
    if (true == itemMatchesKeyAndName(item, key, name)) {
        appendToList((opal_list_t**)&item->value.data.ptr, input, overwrite);
        return ORCM_SUCCESS;
    }

    return ORCM_ERROR;
}

void pugi_impl::appendToList(opal_list_t **srcList, opal_list_t *input, bool overwrite)
{
    if ((NULL == srcList) || (NULL == *srcList)) {
        return;
    }

    if (true == overwrite) {
        SAFE_RELEASE_NESTED_LIST(*srcList);
        *srcList = duplicateList(input);
    }
    else {
        if (NULL == input) {
            return;
        }
        opal_list_join( *srcList, opal_list_get_last(*srcList), input);
    }
}

int pugi_impl::convertXmlNodeToOpalList(xml_node node, opal_list_t *list)
{
    if (!node || NULL == list){
        return ORCM_ERR_BAD_PARAM;
    }
    try {
        addNodeToList(node,list);
        return extractFromEmptyKeyList(list);
    } catch (exception e) {
        return ORCM_ERROR;
    }
}

void pugi_impl::addNodeToList(xml_node node, opal_list_t *list)
{
    if (!node || NULL == list){
        return;
    }
    if (isLeafNode(node)){
        addLeafNodeToList(node,list);
        return;
    }
    opal_list_t *children = OBJ_NEW(opal_list_t);
    if (NULL == children){
        return;
    }
    addNodeAttributesToList(node,children);
    addNodeChildrenToList(node,children);
    string nameString(node.name());
    char *name = strdup(trim(nameString).c_str());
    orcm_util_append_orcm_value(list, name, children, OPAL_PTR, NULL);
    SAFEFREE(name);
}

bool pugi_impl::isLeafNode(xml_node node)
{
    if (node_pcdata == node.first_child().type()){
        return true;
    }
    return false;
}

void pugi_impl::addLeafNodeToList(xml_node node, opal_list_t *list)
{
    if (NULL == list){
        return;
    }
    string name(node.name());
    string value(node.child_value());
    addValuesToList(list, trim(name).c_str(), trim(value).c_str());
}

void pugi_impl::addValuesToList(opal_list_t *list, char const *key, char const *value)
{
    char *keyPtr = strdup(key);
    char *valuePtr = strdup(value);
    if (0 == strcmp("name", key)){
        orcm_util_prepend_orcm_value(list, keyPtr, valuePtr, OPAL_STRING, NULL);
    } else {
        orcm_util_append_orcm_value(list, keyPtr, valuePtr, OPAL_STRING, NULL);
    }
    SAFEFREE(keyPtr);
    SAFEFREE(valuePtr);
}

void pugi_impl::addNodeAttributesToList(xml_node node, opal_list_t *list)
{
    if (NULL == list){
        return;
    }
    for(xml_attribute_iterator ait = node.attributes_begin(); ait != node.attributes_end(); ++ait){
        string name(ait->name());
        string attribute(ait->value());
        addValuesToList(list,trim(name).c_str(),trim(attribute).c_str());
    }
}

void pugi_impl::addNodeChildrenToList(xml_node node, opal_list_t *list)
{
    if (NULL == list){
        return;
    }
    for(xml_node_iterator nit = node.begin(); nit != node.end(); ++nit){
        addNodeToList(*nit, list);
    }
}

int pugi_impl::extractFromEmptyKeyList(opal_list_t *list)
{
    if (NULL == list){
        return ORCM_ERR_BAD_PARAM;
    }
    orcm_value_t *tmp = (orcm_value_t*) opal_list_remove_first(list);
    if (opal_list_is_empty(list) && itemListHasChildren(tmp)){
        opal_list_join(list, opal_list_get_first(list),
                             (opal_list_t *)tmp->value.data.ptr);
        tmp->value.data.ptr = NULL;
        orcm_util_release_nested_orcm_value_list_item(&tmp);
        return ORCM_SUCCESS;
    }
    orcm_util_release_nested_orcm_value_list_item(&tmp);
    return ORCM_ERROR;
}

opal_list_t* pugi_impl::searchInTree(opal_list_t *tree, char const *key, char const* name)
{
    if (NULL == name || 0 == strcmp("", name)){
        return searchKeyInTree(tree,key);
    } else {
        return searchKeyAndNameInTree(tree,key,name);
    }
}

opal_list_t* pugi_impl::searchKeyInTree(opal_list_t *tree, char const *key)
{
    if (NULL == tree) {
        return NULL;
    }
    opal_list_t *result = searchKeyInList(tree,key);
    orcm_value_t *item = NULL;
    OPAL_LIST_FOREACH(item, tree, orcm_value_t) {
        if (itemListHasChildren(item)) {
            opal_list_t *children = searchKeyInTree((opal_list_t *)item->value.data.ptr, key);
            joinLists(&result, &children);
        }
    }
    return result;
}

opal_list_t* pugi_impl::searchKeyInList(opal_list_t *srcList, char const *key)
{
    if (NULL == srcList){
        return NULL;
    }
    opal_list_t *list = duplicateList(srcList);
    if (NULL == list){
        return NULL;
    }
    orcm_value_t *item = NULL, *next = NULL;
    OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_value_t){
        if (0 != strcmp(key,item->value.key)){
            opal_list_remove_item(list,(opal_list_item_t*)item);
            orcm_util_release_nested_orcm_value_list_item(&item);
        }
    }
    if (opal_list_is_empty(list)){
        OPAL_LIST_RELEASE(list);
        return NULL;
    }
    return list;
}

opal_list_t* pugi_impl::duplicateList(opal_list_t *src)
{
    if (NULL == src){
        return NULL;
    }
    opal_list_t *dest = orcm_util_copy_opal_list(src);
    if (NULL != dest){
        orcm_value_t *item = NULL;
        OPAL_LIST_FOREACH(item,dest,orcm_value_t){
            if (itemListHasChildren(item)){
                item->value.data.ptr = duplicateList((opal_list_t*)item->value.data.ptr);
            }
        }
    }
    return dest;
}

bool pugi_impl::itemListHasChildren(orcm_value_t *item)
{
    if (NULL != item && OPAL_PTR == item->value.type){
        return true;
    }
    return false;
}

void pugi_impl::joinLists(opal_list_t **list, opal_list_t **otherList)
{
    if (NULL == *list){
        *list = *otherList;
        *otherList = NULL;
        return;
    }
    if (NULL == *otherList){
        return;
    }
    opal_list_join(*list, opal_list_get_first(*list), *otherList);
    SAFE_RELEASE_NESTED_LIST(*otherList);
}

opal_list_t* pugi_impl::searchKeyAndNameInTree(opal_list_t *tree, char const *key,
                                               char const* name)
{
    if (NULL == tree){
        return NULL;
    }
    opal_list_t *result = searchKeyAndNameInList(tree,key,name);
    orcm_value_t *item = NULL;
    OPAL_LIST_FOREACH(item, tree, orcm_value_t) {
        if (itemListHasChildren(item)) {
            opal_list_t *children = searchKeyAndNameInTree((opal_list_t *)item->value.data.ptr, key,name);
            joinLists(&result, &children);
        }
    }
    return result;
}

opal_list_t* pugi_impl::searchKeyAndNameInList(opal_list_t *srcList, char const *key,
                                               char const* name)
{
    if (NULL == srcList){
       return NULL;
    }
    opal_list_t *list = duplicateList(srcList);
    if (NULL == list){
        return NULL;
    }
    orcm_value_t *item = NULL, *next = NULL;
    OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_value_t){
        if (!itemMatchesKeyAndName(item,key,name)){
            opal_list_remove_item(list,(opal_list_item_t*)item);
            orcm_util_release_nested_orcm_value_list_item(&item);
        }
    }
    if (opal_list_is_empty(list)){
        OPAL_LIST_RELEASE(list);
        return NULL;
    }
    return list;
}

bool pugi_impl::itemMatchesKeyAndName(orcm_value_t* item, char const* key, char const* name)
{
    orcm_value_t *child = NULL;
    if (0 == strcmp(key,item->value.key) && itemListHasChildren(item)){
        OPAL_LIST_FOREACH(child, (opal_list_t*)item->value.data.ptr,orcm_value_t){
            if (0 == strcmp("name",child->value.key) && OPAL_STRING == child->value.type){
                if (NULL == name || 0 == strcmp("", name)) {
                    return false;
                }
                if (0 == strcmp(name, child->value.data.string)){
                    return true;
                }
            }
        }
        if (NULL == name || 0 == strcmp("", name)) {
            return true;
        }
    }
    return false;
}

opal_list_t* pugi_impl::searchInList(opal_list_t *list, char const *key, char const* name)
{
    if (NULL == name || 0 == strcmp("", name)){
        return searchKeyInList(list,key);
    } else {
        return searchKeyAndNameInList(list,key,name);
    }
}

void pugi_impl::unloadFile()
{
    doc.reset();
}

void pugi_impl::freeRoot()
{
    SAFE_RELEASE_NESTED_LIST(root);
}
