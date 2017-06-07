/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "xmlManager.hpp"
using std::string;
using std::cerr;
using std::endl;
using std::cout;

xmlManager::xmlManager() {
}

int xmlManager::open_file(string file_path) {

    pugi::xml_parse_result result =
            document.load_file(file_path.c_str(),
                               pugi::parse_default | pugi::parse_embed_pcdata);

    if (0 != result.status) {
        cerr << "udsensors: Error parsing file \"" << file_path << "\". "
                << result.description() << endl;
        return result.status;
    }
    return 0;
}

pugi::xml_node* xmlManager::get_root_node(string tag) {
    pugi::xml_node* root_node = new pugi::xml_node();
    *root_node = document.first_child();

    if (tag != root_node->name()) {
        *root_node = root_node->child(tag.c_str());
        if (root_node->empty()) {

            cerr << "udsensors: No \"" << tag
                    << "\" tag found in document" << endl;

            delete root_node;
            return NULL;
        }
    }

    return root_node;
}

void xmlManager::print_node(pugi::xml_node source_node, string prefix) {

    cout << prefix << source_node.name() << ": " << source_node.value() << endl;

    for (pugi::xml_attribute_iterator it = source_node.attributes_begin();
         it != source_node.attributes_end();
         it++)
        cout << prefix + "\t" << it->name() << " : " << it->value() << endl;

    for (pugi::xml_node_iterator it = source_node.begin();
         it != source_node.end();
         it++)
        print_node(*it, prefix + "\t");

}
