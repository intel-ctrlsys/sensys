#!/usr/bin/python

#
# Copyright (c) 2015-2017 Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

import sys
import os
import re
from subprocess import Popen, PIPE, STDOUT

cf_name = "orcm-default-config.xml"
cf_dst = ""
help_name = "help-octl.txt"
help_dst = "."

def setup_tests():
    global cf_dst
    global help_dst

    prefix = prefix_search()
    cf_src = os.getenv("srcdir")
    if cf_src:
        cf_src += "/" + cf_name
    else:
        cf_src = cf_name
    cf_dst = prefix + "/etc/" + cf_name
    isThere = os.path.isfile(cf_dst)
    if isThere:
        os.system("sudo mv {0} {0}.bak".format(cf_dst, cf_dst))
    os.system("sudo cp {0} {1}".format(cf_src, cf_dst))

    help_src = prefix + "/share/openmpi/" + help_name
    os.system("cp {0} {1}".format(help_src, help_dst))

def teardown_tests():
    global cf_dst
    os.system("sudo rm {0}".format(cf_dst))
    if os.path.isfile("{0}.bak".format(cf_dst)):
        os.system("sudo mv {0}.bak {0}".format(cf_dst))
    os.system("rm {0}".format(help_name))

def prefix_search():
    prefix = ""
    filename = os.getcwd()+"/../../../../config.status"
    pattern = re.compile(r'S\[\"prefix\"\]=\"(.*)\"')
    with open(filename) as f:
        for line in f:
            match = pattern.search(line)
            if match:
                prefix = match.group(1)

    return prefix

def run_tests(gtest_binary):
    """ Execute gtest binary """
    setup_tests()
    print '+RUNNING: %s'%gtest_binary
    process = Popen(gtest_binary.split(), stdout=PIPE, stderr=PIPE)
    (output, err) = process.communicate()
    retcode = process.wait()

    if retcode != 0:
        print "+STDERR:\n{0}+STDOUT:\n{1}".format(err, output)
    teardown_tests()
    return retcode

if __name__ == '__main__':
    PROG = './octl_tests --gtest_output=xml:octl-log.xml'
    sys.exit(run_tests(PROG))
