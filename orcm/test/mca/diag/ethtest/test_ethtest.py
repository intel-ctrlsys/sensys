#!/usr/bin/python
#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
""" Test wrapper for gtest binaries """
import sys
from subprocess import Popen, PIPE

def get_gtests(gtest_binary):
    """ Get the list of tests from gtest """
    process = Popen([gtest_binary, '--gtest_list_tests'], stdout=PIPE)
    (output, _) = process.communicate()
    exit_code = process.wait()
    if exit_code != 0:
        return []
    lines = output.split('\n')
    fixture = ''
    counter = 0
    tests = []
    for line in lines:
        counter = counter+1
        if counter != 1:
            if line == '':
                continue
            if line[0] != ' ':
                fixture = line
            else:
                test = fixture+line[2:]
                tests.append(test)
    return tests

def execute_test(gtest_binary, test_name):
    """ Execute a given test from a given gtest binary """
    process = Popen([gtest_binary, "--gtest_color=yes",
                     "--gtest_filter=%s"%(test_name)], stdout=PIPE, stderr=PIPE)
    (output, err) = process.communicate()
    ret_value = process.wait()
    return (ret_value, output, err)

def run_tests(gtest_binary):
    """ Executes a gtest_binary and shows a summary """
    tests = get_gtests(gtest_binary)
    ret = 0
    passed = 0
    failed = 0
    print '+RUNNING: %s'%gtest_binary
    for test in tests:
        (ret_value, output, err) = execute_test(gtest_binary, test)
        print ret_value, output, err
        if ret_value != 0:
            print '+STDERR:\n%s'%err
            print '+STDOUT:\n%s'%output
            ret = ret_value
            failed = failed+1
        else:
            passed = passed+1
    print '+TEST SUMMARY PASSED = %d'%(passed)
    print '+TEST SUMMARY FAILED = %d'%(failed)
    return ret

if __name__ == "__main__":
    PROGRAM = './diag_ethtest'
    sys.exit(run_tests(PROGRAM))
