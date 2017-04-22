#
# Copyright (c) 2017      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#

#!/bin/bash

set -ev

if [ "${STATIC_BUILD}" = "true" ]; then
	extra_conf="--with-platform=contrib/platform/intel/hillsboro/orcm-nightly-build-static --with-gtest-incdir=/usr/local/include --with-gtest-libdir=/usr/local/lib"
	test_run="LD_LIBRARY_PATH=/opt/sensys/lib:/opt/sensys/lib/openmpi make check"
else
	extra_conf="--with-platform=contrib/platform/intel/hillsboro/orcm-nightly-build"
	test_run="true"
fi

CMD="cd /sensys-bld/ &&
./autogen.pl &&
./configure --prefix=/opt/sensys $extra_conf &&
make -j4 &&
make install &&
$test_run"

docker run -t -i -v "$PWD":/sensys-bld intelctrlsys/sensys-bld-centos7.3 sh -c "$CMD"

