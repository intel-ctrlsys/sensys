/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "pugi_impl.h"

using namespace pugi;
using namespace std;

int pugi_impl::loadFile()
{
    xml_parse_result result = doc.load_file(file.c_str());
    if (result){
        return convertXmlNodeToOpalList(doc,root);
    }
    return ORCM_ERR_FILE_OPEN_FAILURE;
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
    char *name = strdup(node.name());
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
    addValuesToList(list, node.name(), node.child_value());
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
        addValuesToList(list,ait->name(),ait->value());
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
        return ORCM_ERROR;
    }
    orcm_value_t *tmp = (orcm_value_t*) opal_list_remove_first(list);
    if (opal_list_is_empty(list) && itemListHasChildren(tmp)){
        opal_list_join(list, opal_list_get_first(list),
                             (opal_list_t *)tmp->value.data.ptr);
        tmp->value.data.ptr = NULL;
        SAFE_RELEASE(tmp);
        return ORCM_SUCCESS;
    }
    SAFE_RELEASE(tmp);
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
            SAFE_RELEASE(item);
        }
    }
    if (opal_list_is_empty(list)){
        SAFE_RELEASE(list);
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
    SAFE_RELEASE(*otherList);
    *otherList = NULL;
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
            SAFE_RELEASE(item);
        }
    }
    if (opal_list_is_empty(list)){
        SAFE_RELEASE(list);
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
                if (0 == strcmp(name, child->value.data.string)){
                    return true;
                }
            }
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
    SAFE_RELEASE(root);
}
