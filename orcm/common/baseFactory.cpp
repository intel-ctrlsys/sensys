/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "baseFactory.h"
#include <regex.h>

void baseFactory::setPluginsPath(const char *user_plugins_path)
{
    plugins_path = NULL == user_plugins_path ? DEFAULT_PLUGIN_PATH : user_plugins_path;
    if (*plugins_path.rbegin() != '/') {
        plugins_path = plugins_path + '/';
    }
}

void baseFactory::setPluginsPrefix(const char *user_plugins_prefix)
{
    plugins_prefix = NULL == user_plugins_prefix ? DEFAULT_PLUGIN_PREFIX : user_plugins_prefix;
}


int baseFactory::getPluginFilenames()
{
    DIR *d = NULL;
    int ret = 0;
    compilePluginNameRegex();
    d = opendir(plugins_path.c_str());
    if (d) {
        addPluginsIfPrefixMatch(d);
        closedir(d);
    } else {
        ret = -ENOTDIR;
    }
    return ret;
}

void* baseFactory::openPlugin(std::string plugin)
{
    return (void*)dlopen(plugin.c_str(), RTLD_LAZY);
}

void baseFactory::closePlugin(void *plugin)
{
    dlclose(plugin);
}

void* baseFactory::getPluginSymbol(void* plugin, const char *symbol)
{
    return (void*)dlsym(plugin, symbol);
}

void baseFactory::compilePluginNameRegex(){
    std::string expresion ("^.*\\.so(\\.[[:digit:]]+)*$");
    expresion.insert(1,plugins_prefix);
    regcomp(&regex_comp, expresion.c_str(), REG_EXTENDED|REG_ICASE);
}

bool baseFactory::prefixAndExtMatch(char *d_name)
{
    int regex_res = -1;
    regex_res = regexec(&regex_comp,d_name, 0, NULL, 0);
    return (regex_res == 0);
}

void baseFactory::addPluginsIfPrefixMatch(DIR *d)
{
    struct dirent *entry = NULL;
    while (NULL != (entry = readdir(d))) {
        if (prefixAndExtMatch((entry->d_name))) {
            pluginFilesFound.push_back(formFullPath(entry->d_name));
        }
    }
}

std::string baseFactory::formFullPath(const char *filename)
{
    return std::string(plugins_path + filename);
}

std::string baseFactory::getPluginsPath()
{
    return plugins_path;
}

std::string baseFactory::getPluginsPrefix()
{
    return plugins_prefix;
}
