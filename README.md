<!--

Copyright (c) 2013-2017 Intel Corporation. All rights reserved

$COPYRIGHT$

Additional copyrights may follow

$HEADER$

===========================================================================
-->

# A cluster monitoring system architected for exascale systems

[![Build Status](https://travis-ci.org/intel-ctrlsys/sensys.svg?branch=master)](https://travis-ci.org/intel-ctrlsys/sensys)

Sensys provides resilient and scalable monitoring for resource utilization and node state of health, collecting all the data in a database for subsequent analysis. Sensys includes several loadable plugins that monitor various metrics related to different features present in each node like temperature, voltage, power usage, memory, disk and process information.

# Building and installing

## Dependencies

Dependencies may vary on your system's hardware. Here is the list of all the dependencies in case you want to build Sensys with full support.

Name | Version
-----|--------
Sigar | 1.6.5
libesmtp | 1.0.6
net-snmp | 5.7.3
ipmiutil | 2.9.6
postgresql | 9.2+
unixODBC | 2.3.1
numactl-libs | 2.0.9
openssl | 1.0.1
zeromq | 4.0.5

Sensys features can be switched on and off at build time, check `./configure --help` for a complete list.

## Build and install

Sensys is an autotools based project, to build it the following steps are needed.

1. Run ```./autogen.pl```
2. Run ```./configure --prefix=/opt/sensys --with-platform=contrib/platform/intel/hillsboro/orcm-nightly-build```
3. Run ```make```
4. Run ```make install```
  - Ensure that you have write permissions to the defined prefix as well as root permissions if it is required.

Further build and installation instructions can be found on the [wiki page](https://github.com/intel-ctrlsys/sensys/wiki/2.1-Sensys-Build-and-Installation).

# Getting started and documentation

The Getting Started page can be found in the [Sensys wiki](https://intel-ctrlsys.github.io/sensys/).

# Help
If you have questions with Sensys feel free to [create an issue](https://github.com/intel-ctrlsys/sensys/issues/new).

# License
Sensys is an Open source project released under the [BSD 3-Clause license](LICENSE).
