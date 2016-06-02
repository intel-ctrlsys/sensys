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

using namespace std;

static ipmiCollectorMap ic_map;
static ipmiCollectorVector ic_vector;
static set<string> aggregators;
static set<string>::iterator agg_it;

extern "C" {
    #include "ipmi_parser_interface.h"

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
    bool load_ipmi_config_file(){
        ipmiParser parser;
        ic_map = parser.getIpmiCollectorMap();
        ic_vector = parser.getIpmiCollectorVector();
        get_aggregators();

        return !ic_vector.empty();
    }

    // Initializes the aggregator count
    void start_aggregator_count(){
        agg_it = aggregators.begin();
    }

    // Retrieves the next aggregator name on aggregators 'set'.
    bool get_next_aggregator_name(char** aggregator){
        if (agg_it == aggregators.end()){
            return false;
        }
        *aggregator = strdup(agg_it->c_str());
        agg_it++;
        return true;
    }

    // Retrieves the BMC information from a given node name
    bool get_bmc_info(const char* hostname, ipmi_collector *ic){
        string str_hostname(hostname);
        if (NULL == ic || 0 == ic_map.count(str_hostname)){
            return false;
        }

        strncpy(ic->bmc_address, ic_map[str_hostname].getBmcAddress().c_str(), MAX_STR_LEN-1);
        ic->bmc_address[MAX_STR_LEN-1] = '\0';
        strncpy(ic->user, ic_map[str_hostname].getUser().c_str(), MAX_STR_LEN-1);
        ic->user[MAX_STR_LEN-1] = '\0';
        strncpy(ic->pass, ic_map[str_hostname].getPass().c_str(), MAX_STR_LEN-1);
        ic->pass[MAX_STR_LEN-1] = '\0';
        strncpy(ic->aggregator, ic_map[str_hostname].getAggregator().c_str(), MAX_STR_LEN-1);
        ic->aggregator[MAX_STR_LEN-1] = '\0';
        strncpy(ic->hostname, ic_map[str_hostname].getHostname().c_str(), MAX_STR_LEN-1);
        ic->hostname[MAX_STR_LEN-1] = '\0';
        ic->auth_method = ic_map[str_hostname].getAuthMethod();
        ic->priv_level  = ic_map[str_hostname].getPrivLevel();
        ic->port        = ic_map[str_hostname].getPort();
        ic->channel     = ic_map[str_hostname].getChannel();
        return true;
    }

    // Returns an array of BMC info for a given aggregator
    bool get_bmcs_for_aggregator(char* agg, ipmi_collector** bmc_list, int *n)
    {
        int i = 0;
        string str_agg(agg);
        set<string> str_bmc_list;
        ipmiCollectorVector::iterator it;
        set<string>::iterator bmc_it;
        ipmi_collector* local_bmc_list = NULL;

        for (it = ic_vector.begin(); it != ic_vector.end(); ++it) {
            if (0 == str_agg.compare(it->getAggregator())) {
                str_bmc_list.insert(it->getHostname());
            }
        }

        if (0 == str_bmc_list.size()) {
            return false;
        }

        local_bmc_list = (ipmi_collector*) malloc(str_bmc_list.size()*sizeof(ipmi_collector));
        ORCM_ON_NULL_RETURN_ERROR(local_bmc_list, false);

        for (bmc_it = str_bmc_list.begin(); bmc_it != str_bmc_list.end(); ++bmc_it) {
            (void) get_bmc_info((*bmc_it).c_str(), &local_bmc_list[i++]);
        }

        *n = i;
        *bmc_list = local_bmc_list;

        return true;
    }
}
