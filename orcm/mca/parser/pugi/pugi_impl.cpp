/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "pugi_impl.h"
#define SAFE_OBJ_RELEASE(x) if(NULL != x){ OBJ_RELEASE(x); x = NULL;}

using namespace pugi;
using namespace std;

bool pugi_impl::isLeafNode(xml_node node)
{
    if (node_pcdata == node.first_child().type()){
        return true;
    }
    return false;
}

void pugi_impl::addLeafNodeToList(xml_node node, opal_list_t *list)
{
    if (0 == strcmp("name",node.name())){
        orcm_util_prepend_orcm_value(list, strdup(node.name()),
                                     strdup(node.child_value()), OPAL_STRING, NULL);
    } else {
        orcm_util_append_orcm_value(list, strdup(node.name()),
                                    strdup(node.child_value()), OPAL_STRING, NULL);
    }
}

void pugi_impl::addNodeAttributesToList(xml_node node, opal_list_t *list)
{
   for(xml_attribute_iterator ait = node.attributes_begin(); ait != node.attributes_end(); ++ait){
        if (0 == strcmp("name",ait->name())){
            orcm_util_prepend_orcm_value(list, strdup(ait->name()),
                                         strdup(ait->value()), OPAL_STRING, NULL);
        } else {
            orcm_util_append_orcm_value(list, strdup(ait->name()),
                                        strdup(ait->value()), OPAL_STRING, NULL);
        }
    }
}

void pugi_impl::addNodeChildrenToList(xml_node node, opal_list_t *list)
{
    for(xml_node_iterator nit = node.begin(); nit != node.end(); ++nit){
        addNodeToList(*nit, list);
    }
}

void pugi_impl::addNodeToList(xml_node node, opal_list_t *list)
{
    if (!node){
        return;
    }
    if (isLeafNode(node)){
        addLeafNodeToList(node,list);
        return;
    }
    opal_list_t *children = OBJ_NEW(opal_list_t);
    addNodeAttributesToList(node,children);
    addNodeChildrenToList(node,children);
    orcm_util_append_orcm_value(list, strdup(node.name()), children, OPAL_PTR, NULL);
}

int pugi_impl::extractFromEmptyKeyList(opal_list_t *list)
{
    orcm_value_t *tmp = (orcm_value_t*) opal_list_remove_first(list);
    if (opal_list_is_empty(list) && tmp->value.type == OPAL_PTR){
        opal_list_join(list, opal_list_get_first(list),
                             (opal_list_t *)tmp->value.data.ptr);
        tmp->value.data.ptr = NULL;
        SAFE_OBJ_RELEASE(tmp);
        return ORCM_SUCCESS;
    }
    SAFE_OBJ_RELEASE(tmp);
    return ORCM_ERROR;
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

int pugi_impl::loadFile()
{
    xml_parse_result result = doc.load_file(file.c_str());
    if (result){
        return convertXmlNodeToOpalList(doc,root);
    }
    return ORCM_ERR_FILE_OPEN_FAILURE;
}

void pugi_impl::unloadFile()
{
    doc.reset();
}

opal_list_t* pugi_impl::duplicateList(opal_list_t *src)
{
    if (NULL == src){
        return NULL;
    }
    opal_list_t *dest = orcm_util_copy_opal_list(src);
    orcm_value_t *item;
    OPAL_LIST_FOREACH(item,dest,orcm_value_t){
        if (OPAL_PTR == item->value.type){
            item->value.data.ptr = duplicateList((opal_list_t*)item->value.data.ptr);
        }
    }
    return dest;
}

opal_list_t* pugi_impl::searchKeyInList(opal_list_t *srcList, char const *key)
{
    if (NULL == srcList){
        return NULL;
    }
    opal_list_t *list = duplicateList(srcList);
    orcm_value_t *item, *next;
    OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_value_t){
        if (0 != strcmp(key,item->value.key)){
            opal_list_remove_item(list,(opal_list_item_t*)item);
            SAFE_OBJ_RELEASE(item);
        }
    }
    if (opal_list_is_empty(list)){
        SAFE_OBJ_RELEASE(list);
        return NULL;
    }
    return list;
}

opal_list_t* pugi_impl::searchKeyAndNameInList(opal_list_t *srcList, char const *key,
                                               char const* name)
{
    if (NULL == srcList){
       return NULL;
    }
    opal_list_t *list = duplicateList(srcList);
    orcm_value_t *item, *next, *child;
    bool found;
    OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_value_t){
        found=false;
        if (0 == strcmp(key,item->value.key) && itemListHasChildren(item)){
            OPAL_LIST_FOREACH(child, (opal_list_t*)item->value.data.ptr,orcm_value_t){
                if (0 == strcmp("name",child->value.key) && OPAL_STRING == child->value.type){
                    if (0 == strcmp(name, child->value.data.string)){
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found){
            opal_list_remove_item(list,(opal_list_item_t*)item);
            SAFE_OBJ_RELEASE(item);
        }
    }
    if (opal_list_is_empty(list)){
        SAFE_OBJ_RELEASE(list);
        return NULL;
    }
    return list;
}

bool pugi_impl::isFullDocumentListRequested(opal_list_item_t *start, char const *key,
                                            char const* name)
{
    if ( NULL == start && (NULL == key || 0 == strcmp("", key)) &&
         (NULL == name || 0 == strcmp("", name))){
        return true;
    }
    return false;
}

bool pugi_impl::isKeySearchedFromGivenStart(opal_list_item_t *start, char const *key)
{
    if ( NULL != start && (NULL != key && 0 != strcmp("", key))){
        return true;
    }
    return false;
}

bool pugi_impl::isKeySearchedFromDocumentStart(opal_list_item_t *start, char const *key)
{
    if ( NULL == start && (NULL != key && 0 != strcmp("", key))){
        return true;
    }
    return false;
}

bool pugi_impl::itemListHasChildren(orcm_value_t *item)
{
    if (OPAL_PTR == item->value.type){
        return true;
    }
    return false;
}

opal_list_t* pugi_impl::searchInList(opal_list_t *list, char const *key,char const* name)
{
    if (NULL == name || 0 == strcmp("", name)){
        return searchKeyInList(list,key);
    } else {
        return searchKeyAndNameInList(list,key,name);
    }
}

opal_list_t* pugi_impl::retrieveSection(opal_list_item_t *start, char const *key,
                                        char const* name)
{
    if (isFullDocumentListRequested(start, key, name)){
        return duplicateList(root);
    }
    if (isKeySearchedFromDocumentStart(start, key)){
        return searchInList(root,key,name);
    }
    if (isKeySearchedFromGivenStart(start, key)){
        orcm_value_t *s = (orcm_value_t *)start;
        if (itemListHasChildren(s)){
            return searchInList((opal_list_t*)(s->value.data.ptr), key,name);
        }
    }
    return NULL;
}

void pugi_impl::freeRoot()
{
    SAFE_OBJ_RELEASE(root);
}
