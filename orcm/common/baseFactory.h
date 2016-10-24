/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BASE_FACTORY_H
#define BASE_FACTORY_H

#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <regex.h>

#define DEFAULT_PLUGIN_PATH "/usr/lib64"
#define DEFAULT_PLUGIN_PREFIX "libudplugin_"

typedef void* (*pluginInstance)(void);

class baseFactory {

private:
    bool prefixAndExtMatch(char *d_name);
    void compilePluginNameRegex();

    regex_t regex_comp;

public:
    void setPluginsPath(const char *user_plugins_path);
    void setPluginsPrefix(const char *user_plugins_prefix);
    std::string getPluginsPath(void);
    std::string getPluginsPrefix(void);
    int getPluginFilenames(void);
    void addPluginsIfPrefixMatch(DIR *d);
    void* getPluginSymbol(void*, const char *symbol);
    void* openPlugin(std::string plugin);
    void closePlugin(void *plugin);
    std::string formFullPath(const char *filename);

    std::vector<std::string> pluginFilesFound;
    std::string plugins_path;
    std::string plugins_prefix;

protected:
    baseFactory(){};
    virtual ~baseFactory(){};
};

#endif
