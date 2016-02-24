/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_H
#define PARSER_H

#include<stdlib.h>

class parser {
    public:

        parser(std::string file) : parser(file.c_str()) {};
        parser(char const* file="")  { this->file = file; };
        ~parser() { unloadFile(); };

        virtual int loadFile(){ return 0; };
        virtual int getValue(char *value, char const* key) { return 0; };
        virtual int getArray(char **array, int* size, char const* key) { return 0; };

    protected:

        char const* file;
        virtual void unloadFile() {};
};

#endif

