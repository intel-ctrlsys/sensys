/*
 * Copyright (c) 2016 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_PUGI_H
#define PARSER_PUGI_H

#include "orcm/mca/parser/parser.h"
#include "opal/class/opal_list.h"

BEGIN_C_DECLS

ORCM_DECLSPEC extern orcm_parser_base_component_t mca_parser_pugi_component;
ORCM_DECLSPEC extern orcm_parser_base_module_t orcm_parser_pugi_module;

/**
 * Opens an xml file using pugixml.
 *
 * @param[in] file: file name to be parsed.
 *
 * @returns A file id if the file can be opened.
 *          ORCM_ERR_FILE_OPEN_FAILURE if it fails.
 *
 * The returned file id will be used in other functions to perform operations
 * on that file.
 * Several instances of the file can be opened simultaneously, but each of
 * them will have a different file id.
 */
extern int pugi_open(char const* file);

/**
 * Closes an xml file opened by pugi_open function.
 *
 * @param[in] file_id: file id returned by pugi_open.
 *
 * @returns ORCM_SUCCESS or
 *          ORCM_ERROR if it fails or if the file id is not valid.
 */
extern int pugi_close(int file_id);

/**
 * Parses an xml file and retrieves all its content.
 *
 * @param[in] file_id: file id returned by pugi_open.
 *
 * @returns A list of lists with the content of the xml file to be parsed.
 *
 * The list is formed by orcm_value_t objects using their key as the tag names
 * of the xml file, and the value.data will have the content of the xml tags.
 * The value.data can be a string if the xml tag doesn't have children nor
 * attributes inside, or it can be a pointer to another list that will have
 * its attributes and children.
 */
extern opal_list_t* pugi_retrieve_document(int file_id);

/**
 * Parses an xml file and retrieves all the sections that match the
 * key (key/name) provided.
 *
 * @param[in] file_id: file id returned by pugi_open.
 * @param[in] key:     xml tag that will be searched across the xml document.
 * @param[in] name:    it can be null, or empty string if it needs to be
 *                     ignored; otherwise, the result items need to match their
 *                     name attribute with the value of this param.
 *
 * @returns A list of lists that match the given criteria (key and name)
 *          from the full xml.
 *
 * This function will go throw all the document tree and will look for the
 * given tag(key). If name is provided (different than null or empty string),
 * it will look for the given tag that has the given name as "name" attribute
 * in the xml file.
 * The returned list will have orcm_value_t objects that have the key provided
 * with all their content (children) inside the value.data field.
 * All items matching the key will be returned as items of the outer list.
 */
extern opal_list_t* pugi_retrieve_section(int file_id, char const* key,
                                          char const* name);

/**
 * Retrieves all the sections that match the key (key/name) from a given
 * item list.
 *
 * @param[in] file_id: file id returned by pugi_open.
 * @param[in] start:   opal_list_item_t that represents an orcm_value_t which
 *                     has a list in which the tag (key) and name will be
 *                     searched.
 * @param[in] key:     xml tag that will be searched.
 * @param[in] name:    it can be null, or empty string if it needs to be
 *                     ignored; otherwise, the result items need to match
 *                     their name attribute with the value of this param.
 *
 * @returns A list of lists that match the given criteria (key and name)
 *          from the list item provided in start param.
 *
 * Start is an opal_list_item_t that represents an orcm_value_t representing
 * an xml tag. The tag might have children inside, that are represented by a
 * list stored in the orcm_value of the start param.
 * If the tag has children, the function will return a list with the items
 * (orcm_value_t) that match the key and name (if provided). Note that name is
 * used to reference the value of the "name" attribute in an xml node/tag.
 * This function will search only in the first level children. To search in
 * all the tree/document use pugi_retrieve_section function.
 */
extern opal_list_t* pugi_retrieve_section_from_list(int file_id,
                                          opal_list_item_t *start, char const* key,
                                          char const* name);

/**
 * Writes a list to an xml file at the element node that matches the
 * key (key/name) provided.
 *
 * @param[in] file_id: file id returned by pugi_open.
 * @param[in] input:   Input to be written to XML. Parser will handle freeing of
 *                     the memory in input list.
 * @param[in] key:     xml tag that will be searched across the xml document.
 * @param[in] name:    it can be null, or empty string if it needs to be
 *                     ignored; otherwise, the result items need to match their
 *                     name attribute with the value of this param.
 * @param[in] overwrite: If this variable is set to true, the contents of XML will
 *                     be overwritten with the contents in input at specified key/name
 *                     location. Please use this variable with caution. If you send NULL
 *                     input at NULL key, then the entire XML will be overwritten
 *
 * @returns ORCM_SUCCESS- when operation is success or an appropriate failure.
 *
 * This function will go throw all the document tree and will look for the
 * given tag(key). If name is provided (different than null or empty string),
 * it will look for the given tag that has the given name as "name" attribute
 * in the xml file. Then, it will append the input list at the specified location
 * If key is NULL or empty, then the list is added at end of the XML
 */
extern int pugi_write_section(int file_id, opal_list_t *input,
                              char const *key, char const* name, bool overwrite);

/**
 * Cleans up memory when the module is closed.
 *
 * Deletes objects from unclosed files.
 *
 */
extern void pugi_finalize(void);
END_C_DECLS

#endif /* PARSER_PUGI_H */
