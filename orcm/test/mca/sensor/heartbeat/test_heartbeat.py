#!/usr/bin/python
#
# Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
""" Script to execute Gtest Tests """

import sys
from subprocess import Popen, PIPE

def run_tests(gtest_binary):
    """ Execute gtest binary """
    print '+RUNNING: %s'%gtest_binary
    process = Popen(gtest_binary.split(), stdout=PIPE, stderr=PIPE)
    (output, err) = process.communicate()
    retcode = process.wait()

    if retcode != 0:
        print "+STDERR:\n{0}+STDOUT:\n{1}".format(err, output)
    return retcode

if __name__ == '__main__':
    PROG = './heartbeat_tests --gtest_output=xml:heartbeat-log.xml'
    sys.exit(run_tests(PROG))
