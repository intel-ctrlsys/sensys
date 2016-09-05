/*
 * Copyright (c) 2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "baseFactory.h"

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

void baseFactory::addPluginsIfPrefixMatch(DIR *d)
{
    struct dirent *entry = NULL;
    while (NULL != (entry = readdir(d))) {
        if (!strncmp(entry->d_name, plugins_prefix.c_str(), strlen(plugins_prefix.c_str()))) {
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
