#!/usr/bin/python
#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
""" Script to execute Gtest Tests """

import sys
from subprocess import Popen, PIPE

def get_gtests(gtest_binary):
    """ Get list of tests to be executed """
    process = Popen([gtest_binary, '--gtest_list_tests'], stdout=PIPE)
    (output, _) = process.communicate()
    exit_code = process.wait()
    if exit_code != 0:
        return []
    lines = output.split('\n')
    fixture = ''
    count = 0
    tests = []
    for line in lines:
        count = count+1
        if count != 1:
            if line == '':
                continue
            if line[0] != ' ':
                fixture = line
            else:
                test = fixture+line[2:]
                tests.append(test)
    return tests

def execute_test(gtest_binary, test_name):
    """ Execute a given test """
    process = Popen([gtest_binary, "--gtest_color=yes", "--gtest_filter=%s"%(test_name)],
                    stdout=PIPE, stderr=PIPE)
    (output, err) = process.communicate()
    retv = process.wait()
    return (retv, output, err)

def run_tests(gtest_binary):
    """ Execute gtest binary """
    tests = get_gtests(gtest_binary)
    ret = 0
    passed = 0
    failed = 0
    print '+RUNNING: %s'%gtest_binary
    for test in tests:
        (retv, output, err) = execute_test(gtest_binary, test)
        if retv != 0:
            print '+STDERR:\n%s'%err
            print '+STDOUT:\n%s'%output
            ret = retv
            failed = failed+1
        else:
            passed = passed+1
    print '+TEST SUMMARY PASSED = %d'%(passed)
    print '+TEST SUMMARY FAILED = %d'%(failed)
    return ret

if __name__ == '__main__':
    PROG = './util_tests'
    sys.exit(run_tests(PROG))
