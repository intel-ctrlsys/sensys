/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_MCA_ANALYTICS_EXTENSION_PLUGINS_ANALYTICS_EXAMPLE_H_
#define ORCM_MCA_ANALYTICS_EXTENSION_PLUGINS_ANALYTICS_EXAMPLE_H_

#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/mca/analytics/base/analytics_factory.h"

class Average : public Analytics {
    public:
        Average();
        virtual ~Average();
        int analyze(DataSet& data_set);
        static Analytics* creator();

    private:
        Average(Average const &);
};

#endif /* ORCM_MCA_ANALYTICS_EXTENSION_PLUGINS_ANALYTICS_EXAMPLE_H_ */
