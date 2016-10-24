#!/bin/bash
#
# Copyright (c) 2015      Intel Corporation. All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

export LANG=C
git ls-files | grep -v pdf | grep -v 3in | tr '\n' '\0' | xargs -0 sed -i '' -E "s/[[:space:]]*$//"

