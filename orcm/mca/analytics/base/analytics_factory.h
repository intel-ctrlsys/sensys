/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_H_
#define ORCM_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_H_

#include <string>
#include <map>
#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/util/baseFactory.h"

#define ANALYTICS_SUCCESS 0
#define ANALYTICS_ERROR 1

typedef Analytics* (*create_obj) (void);

class AnalyticsFactory : public baseFactory {
    public:
        int search(const char *directory, const char *plugin_prefix);
        void cleanup();
        void setPluginCreator(std::string plugin_name, create_obj);
        Analytics* createPlugin(std::string plugin_name);
        bool checkPluginExisit(const char *plugin_name);
        static AnalyticsFactory* getInstance();

    private:
        std::map<std::string, create_obj> plugins;
        AnalyticsFactory() {};
        virtual ~AnalyticsFactory() {};
        int findAndRegistPlugins(void);
        int registPlugins(void);
        int openAndSetPluginCreator(std::string plugin_name);
};

#endif /* ORCM_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_H_ */
