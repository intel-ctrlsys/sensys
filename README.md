<!--

Copyright (c) 2013-2017 Intel Corporation. All rights reserved

$COPYRIGHT$

Additional copyrights may follow

$HEADER$

===========================================================================
-->

# A cluster monitoring system architected for exascale systems

[![Build Status](https://travis-ci.org/intel-ctrlsys/sensys.svg?branch=v0.29)](https://travis-ci.org/intel-ctrlsys/sensys)

Sensys provides resilient and scalable monitoring for resource utilization and node state of health, collecting all the data in a database for subsequent analysis. Sensys includes several loadable plugins that monitor various metrics related to different features present in each node like temperature, voltage, power usage, memory, disk and process information.

# Building and installing

## Dependencies

Dependencies may vary on your system's hardware. Here is the list of all the dependencies in case you want to build Sensys with full support.

Name | Version
-----|--------
Sigar | 1.6.5
libesmtp | 1.0.6
net-snmp | 5.7.3
net-snmp-devel | 5.7.3
ipmiutil | 2.9.6
ipmiutil-devel | 2.9.6
postgresql | 9.3
postgresql-devel | 9.3
unixODBC | 2.3.1
numactl-libs | 2.0.9
openssl | 1.0.1
zeromq | 4.0.5
python | 2.7
python-psycopg2 | 2.5.1
python-sqlalchemy | 0.9.8
python-alembic | 0.8.3

Sensys features can be switched on and off at build time, check `./configure --help` for a complete list.

## Build and install

Sensys is an autotools based project, to build it the following steps are needed.

1. Run ```./autogen.pl```
2. Run ```./configure --prefix=/opt/sensys --with-platform=contrib/platform/intel/hillsboro/orcm-nightly-build```
3. Run ```make```
4. Run ```make install```
  - Ensure that you have write permissions to the defined prefix as well as root permissions if it is required.

Further build and installation instructions can be found on the [wiki page](https://github.com/intel-ctrlsys/sensys/wiki/2.1-Sensys-Build-and-Installation).

# Getting started

Sensys will install all the files on the prefix location, in this example under ```/opt/sensys```. You will find the following directory structure.

```
/opt/sensys/
    bin/
    etc/
    include/
    lib/
    share/
```

- **bin** : This directory holds the binaries of sensys: the scheduler ```orcmsched```, the daemon ```orcmd``` and the CLI ```octl```, as well as other helpers.
- **etc** : Two configuration files can be found here, the ```orcm-site.xml``` and the ```openmpi-mca-params.conf```.
- **include** : Headers needed for compilation of new plugins.
- **lib** : This folder contains the main libraries needed by Sensys as well as all the plugins that can be loaded into the system.
- **share** : Contains documentation of the different components of Sensys.

## The scheduler ```orcmsched```

```orcmsched``` maintains the status of all monitoring daemons running in the system and is the single point gateway for issuing commands and receiving responses from the monitoring nodes. It is expected to run ```orcmsched``` on a head node and it should be launched during the cluster boot up process.

Usage documentation can be found on [orcmsched wiki page](https://github.com/intel-ctrlsys/sensys/wiki/3.3-orcmsched)

## The monitoring daemon ```orcmd```

The Sensys daemon collects RAS monitoring data from compute nodes and it should be launched during the cluster boot up process. ```orcmd``` has two modes of operation:

1. As aggregator: The aggregator mode is intended to gather all telemetry data from compute nodes that belongs to the aggregator. This data can be used by the analytics framework to filter information and send alerts. The aggregator mode is in charge of storing data in the database as well as gathering Out of Band metrics from sources like SNMP or IPMI.
1. As compute node: In this mode ```orcmd``` gathers all the monitoring data and sends it to the configured aggregator.

Further information on the usage of ```orcmd``` can be found [here](https://github.com/intel-ctrlsys/sensys/wiki/3.2-orcmd)

## Sensys configuration

All the Sensys runtime daemons need to know the node hierarchy within the cluster. This configuration is a XML file with the syntax detailed on the [wiki page](https://github.com/intel-ctrlsys/sensys/wiki/3.4-Sensys-CFGI-User-Guide).

# Documentation
The full Sensys documentation can be found [here](https://github.com/intel-ctrlsys/sensys/wiki).

# Help
If you have questions with Sensys feel free to [create an issue](https://github.com/intel-ctrlsys/sensys/issues/new).

# License
Sensys is an Open source project released under the [BSD 3-Clause license](LICENSE).
