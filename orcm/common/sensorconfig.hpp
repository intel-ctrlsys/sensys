/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SENSORCONFIG_HPP
#define SENSORCONFIG_HPP
#include <string>
#include <list>
#include <memory>
#include "parsers/pugixml/pugixml.hpp"

#define UDSENSORS_TAG "udsensors"
#define CONSOLE_LOG_TAG "console_log"

class sensorConfigNodeInterface {
public:

    sensorConfigNodeInterface(void);

    virtual ~sensorConfigNodeInterface(void);

    virtual std::string
    get_attribute(std::string) = 0;

    virtual std::list< std::pair<std::string, std::string> >
    get_attributes_list(void) = 0;

    virtual std::list< std::string >
    get_attributes_list(std::string) = 0;

    virtual std::list< std::shared_ptr<sensorConfigNodeInterface> >
    get_object_list(void) = 0;

    virtual std::list< std::shared_ptr<sensorConfigNodeInterface> >
    get_object_list(std::string) = 0;

    virtual std::list< std::shared_ptr<sensorConfigNodeInterface> >
    get_object_list_by_attribute(std::string, std::string, std::string) = 0;

    virtual std::string
    get_tag(void) = 0;
};

extern "C" {

    enum File_types {
        XML, UNKNOWN
    };

    std::shared_ptr<sensorConfigNodeInterface>
    getConfigFromFile(std::string file_path, File_types file_type, std::string tag);

}

typedef std::list< std::pair<std::string, std::string> >
attributes_list_t;

typedef std::list< std::pair<std::string, std::string> >::iterator
attributes_list_iterator_t;

typedef std::list< std::shared_ptr<sensorConfigNodeInterface> >
objects_list_t;

typedef std::list< std::shared_ptr<sensorConfigNodeInterface> >::iterator
objects_list_iterator_t;

typedef std::list< std::string>
string_list_t;

typedef std::list< std::string> ::iterator
string_list_iterator_t;

typedef std::shared_ptr<sensorConfigNodeInterface>
sensorConfigNodePointer;

#endif /* SENSORCONFIG_HPP */

