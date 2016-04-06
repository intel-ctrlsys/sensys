/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <set>
#include "orcm/mca/sensor/ipmi/ipmi_parser.h"

static ipmiCollectorMap ic_map;
static ipmiCollectorVector ic_vector;
static set<string> aggregators;
static set<string>::iterator agg_it;

extern "C" {
    #include "ipmi_parser_interface.h"

    static ipmi_collector *collector;

    // Obtains the list of aggregator names from IPMI collector
    static void get_aggregators(){
        ipmiCollectorVector::iterator it;
        for (it = ic_vector.begin(); it != ic_vector.end(); ++it){
            aggregators.insert(it->getAggregator());
        }
        start_aggregator_count();
    }

    // Parses the file and retrieves the IPMI collector 'map' & 'vector'
    // and 'set' of aggregators.
    void load_ipmi_config_file(){
        ipmiParser parser;
        ic_map = parser.getIpmiCollectorMap();
        ic_vector = parser.getIpmiCollectorVector();
        get_aggregators();
    }

    // Initializes the aggregator count
    void start_aggregator_count(){
        agg_it = aggregators.begin();
    }

    // Retrieves the next aggregator name on aggregators 'set'.
    bool get_next_aggregator_name(char* aggregator){
        if (agg_it == aggregators.end()){
            return false;
        }
        strncpy(aggregator, agg_it->c_str(), MAX_STR_LEN-1);
        agg_it++;
        return true;
    }

    // Retrieves the BMC information from a given node name
    bool get_bmc_info(char* hostname, ipmi_collector *ic){
        string str_hostname(hostname);
        if (NULL == ic || 0 == ic_map.count(str_hostname)){
            return false;
        }

        collector = ic;

        strncpy(collector->bmc_address, ic_map[str_hostname].getBmcAddress().c_str(), MAX_STR_LEN-1);
        strncpy(collector->user, ic_map[str_hostname].getUser().c_str(), MAX_STR_LEN-1);
        strncpy(collector->pass, ic_map[str_hostname].getPass().c_str(), MAX_STR_LEN-1);
        strncpy(collector->aggregator, ic_map[str_hostname].getAggregator().c_str(), MAX_STR_LEN-1);
        strncpy(collector->hostname, ic_map[str_hostname].getHostname().c_str(), MAX_STR_LEN-1);
        collector->auth_method = ic_map[str_hostname].getAuthMethod();
        collector->priv_level  = ic_map[str_hostname].getPrivLevel();
        collector->port        = ic_map[str_hostname].getPort();
        collector->channel     = ic_map[str_hostname].getChannel();
        return true;
    }
}
