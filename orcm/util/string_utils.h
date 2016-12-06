/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <iostream>
#include <algorithm>
#include <regex.h>
#include <vector>
#include <string.h>
#include <sstream>
#include <iomanip>

using namespace std;

/**
 * @brief Function that removes the white spaces from the beginning
 *        of a string.
 *
 * @param s String from which you want to remove the white spaces.
 *
 * @return String without white spaces at the beginning.
 */
static inline string &ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
    return s;
}

/**
 * @brief Function that removes the white spaces from the end
 *        of a string.
 *
 * @param s String from which you want to remove the white spaces.
 *
 * @return String without white spaces at the end.
 */
static inline string &rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

/**
 * @brief Function that removes the white spaces from the beginning and the end
 *        of a string.
 *
 * @param s String from which you want to remove the white spaces.
 *
 * @return String without white spaces at the beginning and the end.
 */
static inline string &trim(string &s) {
    return ltrim(rtrim(s));
}

/**
 * @brief Function that splits a string into several components using a delimiter.
 *
 * @param str String that you want to split into several elements.
 *
 * @param delimiter Pattern that indicates when an element ends and another begins.
 *
 * @param trim_tokens Flag that indicates if you want to remove the white spaces from each element or not.
 *
 * @return A vector with the splitted elements.
 */
static inline vector<string> splitString(string str, string delimiter=",", bool trim_tokens=true){
    vector<string> token_vector;
    size_t pos = 0;
    string token;

    while ((pos = str.find(delimiter)) != string::npos) {
        token = str.substr(0, pos);
        str.erase(0, pos + 1);
        if(trim_tokens)
            trim(token);
        token_vector.push_back(token);
    }

    if(trim_tokens)
        trim(str);
    token_vector.push_back(str);

    return token_vector;
}

/**
 * @brief Function that joins two vectors without duplicates.
 *
 * @param main_vector Vector where you want the join to be done.
 *
 * @param join_vector Vector that you want to integrate to the main_vector.
 *
 * @return A reference to the main_vector with the join done.
 */
static inline vector<string> &unique_str_vector_join(vector<string> &main_vector, vector<string> join_vector) {
    for (vector<string>::iterator it=join_vector.begin(); it != join_vector.end(); ++it){
        if (find(main_vector.begin(), main_vector.end(), *it) == main_vector.end())
            main_vector.push_back(*it);
    }

    return main_vector;
}

static inline bool stringMatchRegex(string str, string pattern){
    regex_t regex_comp;
    regcomp(&regex_comp, pattern.c_str(), REG_ICASE|REG_EXTENDED);
    return !regexec(&regex_comp, str.c_str(), 0, NULL, 0);
}

static inline char** convertStringVectorToCharPtrArray(vector<string> v){
    char **charptrArray;
    if (v.empty()) return NULL;
    charptrArray = (char**) malloc(v.size()*sizeof(char*));
    if (!charptrArray) return NULL;
    for (int i=0; i<v.size();i++){
        charptrArray[i] = strdup(v[i].c_str());
    }
    return charptrArray;
}

/**
 * @brief Function that turns an integer hexadecimal value into a string.
 * The hexadecimal value is represented on pairs of digits. It will fill
 * missing digits '0'.
 *
 * @param hex_data An integer variable holding the hexadecimal
 *
 * @return The resulting string.
 */
static inline string hex_to_str(uint32_t hex_data)
{
    ostringstream oss;
    oss << hex << setfill('0') << setw(2)<< hex_data;
    return oss.str();
}

/**
 * @brief Function that turns an integer hexadecimal value into a string
 *
 * @param hex_data An integer variable holding the hexadecimal
 *
 * @return The resulting string.
 */
static inline string hex_to_str_no_fill(uint32_t hex_data)
{
    ostringstream oss;
    oss << hex << hex_data;
    return oss.str();
}


/**
 * @brief Function that turns a printable datatype into a string
 *
 * @param t_data The input data
 *
 * @return The resulting string.
 */
template <typename T>
static inline string cast_to_str(T t_data)
{
    ostringstream oss;
    oss << t_data;
    return oss.str();
}

#endif //PARSER_UTILS_H
