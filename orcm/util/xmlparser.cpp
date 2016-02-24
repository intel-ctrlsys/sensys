/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "xmlparser.h"

using namespace pugi;
using namespace std;

xmlparser::~xmlparser(){
    unloadFile();
}

int xmlparser::loadFile(){
    xml_parse_result result = doc.load_file(file);
    if (result){
        return ORCM_SUCCESS;
    }
    return ORCM_ERR_FILE_OPEN_FAILURE;
}

void xmlparser::unloadFile(){
    doc.reset();
}

bool xmlparser::isKeyWellFormed(char const* key){
    string k = string(key);
    string regex = ".*" ;
    return stringMatchRegex(k,regex);
}

vector< vector<string> > xmlparser::splitKey(char const *key){
    vector< vector<string> > v;
    vector<string> tags;
    if (isKeyWellFormed(key)){
        tags = splitString(string(key),string(":"));
        for (vector<string>::iterator it=tags.begin(); it!=tags.end(); it++){
             v.push_back(splitString(*it,string("=")));
        }
    }
    return v;
}

bool xmlparser::keyEndsWithName(char const *key){
    vector< vector<string> > v = splitKey(key);
    if (!v.empty() && 2 == v.back().size()){
        return true;
    }
    return false;
}


xml_node xmlparser::findNode(char const* key){
    xml_node empty_node;
    xml_node node=doc;
    vector< vector<string> > v = splitKey(key);
    if (v.empty()) {
        return empty_node;
    }
    for (vector< vector<string> >::iterator it=v.begin(); it!=v.end(); it++){
        if (1 == (*it).size()){
            node = getNodeWithoutName(node,(*it)[0]);
        } else if (2 == (*it).size()){
            node = getNodeWithName(node,(*it)[0],(*it)[1]);
        } else {
            return empty_node;
        }
    }
    return node;
}

xml_node xmlparser::getNodeWithoutName(xml_node node,string tag_name){
    return node.child(tag_name.c_str());
}

xml_node xmlparser::getNodeWithName(xml_node node, string tag_name, string name){
    return node.find_child_by_attribute(tag_name.c_str(),"name",name.c_str());
}

char* xmlparser::getValueFromNode(xml_node node){
    string value;
    if (!node) {
        return NULL;
    }
    if(node_pcdata == node.first_child().type()){
        value = string(node.child_value());
        trim(value);
        return strdup(value.c_str());
    }
    return NULL;
}

int xmlparser::getValue(char** ptrToValue, char const* key){
    *ptrToValue = NULL;
    xml_node node = findNode(key);
    *ptrToValue = getValueFromNode(node);
    if (NULL != *ptrToValue) {
        return ORCM_SUCCESS;
    }
    return ORCM_ERR_NOT_FOUND;
}

int xmlparser::getArray(char*** ptrToArray, int* size, char const* key){
    *ptrToArray = NULL;
    *size = 0;
    vector<string> names;
    xml_node node = keyEndsWithName(key)? findNode(key).first_child() : findNode(key);
    for(; node ; node = node.next_sibling(node.name())){
        if(node.attribute("name")) {
            names.push_back(string(node.attribute("name").value()));
        }
    }
    *ptrToArray = convertStringVectorToCharPtrArray(names);
    if (NULL != *ptrToArray) {
        *size = names.size();
        return ORCM_SUCCESS;
    }
    return ORCM_ERR_NOT_FOUND;
}
