/*
 * Copyright (c) 2017   Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#ifndef XMLFILES_H
#define XMLFILES_H

#ifndef XML_TEST_FILE
#define XML_TEST_FILE "/tmp/udconfig.xml"
#endif /* XML_TEST_FILE */

#include <string>
#include <iostream>
#include <fstream>

class xmlFiles {
public:
    std::string DEFAULT_FILE;
    std::string NON_PARSABLE_FILE;
    std::string NO_UDSENSOR_TAG_FILE;
    std::string ROOT_ATTRIBUTES_FILE;

    xmlFiles() {
        DEFAULT_FILE =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>                 \n"
                "<udsensors>                                                 \n"
                "    <dfx id=\"myid\">                                       \n"
                "        <name>sku</name>                                    \n"
                "        <enabled>yes</enabled>                              \n"
                "        <injection_rate>10</injection_rate>                 \n"
                "        <error_type>connection</error_type>                 \n"
                "        <complicated_thing name=\"unknown name\">           \n"
                "            <complicated_child> ugly </complicated_child>   \n"
                "        </complicated_thing>                                \n"
                "    </dfx>                                                  \n"
                "                                                            \n"
                "    <dfx>                                                   \n"
                "        <name>bky</name>                                    \n"
                "        <enabled>no</enabled>                               \n"
                "        <injection_rate>10</injection_rate>                 \n"
                "        <n_failures>3</n_failures>                          \n"
                "    </dfx>                                                  \n"
                "                                                            \n"
                "    <aggregator>                                            \n"
                "        <name>agg01</name>                                  \n"
                "        <bky_list>board1</bky_list>                         \n"
                "    </aggregator>                                           \n"
                "                                                            \n"
                "    <lustre>                                                \n"
                "        <name>lustre</name>                                 \n"
                "        <path>/proc/fs/lustre</path>                        \n"
                "    </lustre>                                               \n"
                "                                                            \n"
                "    <bky_board name=\"ibute\" name=\"bute2\" attri=\"bute\">\n"
                "        <name>board01</name>                                \n"
                "        <address>10.219.10.02</address>                     \n"
                "        <port>55819</port>                                  \n"
                "        <node_ports>55820,55821,55822,55823</node_ports>    \n"
                "        <user>bky_username</user>                           \n"
                "        <pass>bky_password</pass>                           \n"
                "    </bky_board>                                            \n"
                "                                                            \n"
                "    <mky_board>                                             \n"
                "        <name>board01</name>                                \n"
                "        <address>10.219.10.02</address>                     \n"
                "        <user>bky_username</user>                           \n"
                "        <pass>bky_password</pass>                           \n"
                "    </mky_board>                                            \n"
                "</udsensors>                                                \n"
                ;

        NON_PARSABLE_FILE =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>                 \n"
                "<udsensors>                                                 \n"
                "    <dfx id=\"myid\">                                       \n"
                "        <name>sku</name>                                    \n"
                "        <enabled>yes</enabled>                              \n"
                "    </dfx>                                                  \n"
                "                                                            \n"
                "    <aggregator>                                            \n"
                "        <name>agg01</name>                                  \n"
                "        <path>/proc/fs/lustre</path>                        \n"
                "    </lustre>                                               \n"
                "                                                            \n"
                "    <bky_board name=\"ibute\" name=\"bute2\" attri=\"bute\">\n"
                "        <name>board01</name>                                \n"
                "        <address>10.219.10.02</address>                     \n"
                "        <port>55819</port>                                  \n"
                "</udsensors>                                                \n"
                ;

        NO_UDSENSOR_TAG_FILE =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>                 \n"
                "<config>                                                    \n"
                "    <dfx id=\"myid\">                                       \n"
                "        <name>sku</name>                                    \n"
                "        <udsensors>yes</udsensors>                          \n"
                "        <injection_rate>10</injection_rate>                 \n"
                "        <error_type>connection</error_type>                 \n"
                "        <complicated_thing name=\"unknown name\">           \n"
                "            <complicated_child> ugly </complicated_child>   \n"
                "        </complicated_thing>                                \n"
                "    </dfx>                                                  \n"
                "</config>                                                   \n"
                ;

        ROOT_ATTRIBUTES_FILE =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>                 \n"
                "<udsensors test_attribute_one=\"one\">                      \n"
                "    <dfx id=\"myid\">                                       \n"
                "        <name>sku</name>                                    \n"
                "        <udsensors>yes</udsensors>                          \n"
                "        <injection_rate>10</injection_rate>                 \n"
                "        <error_type>connection</error_type>                 \n"
                "        <complicated_thing name=\"unknown name\">           \n"
                "            <complicated_child> ugly </complicated_child>   \n"
                "        </complicated_thing>                                \n"
                "    </dfx>                                                  \n"
                "    <test_attribute_two>two</test_attribute_two>            \n"
                "</udsensors>                                                \n"
                ;

    }

    void create_test_file(std::string content) {
        std::ofstream test_file(XML_TEST_FILE);
        test_file << content << std::endl;
        test_file.close();
    }
};

#endif /* XMLFILES_H */

