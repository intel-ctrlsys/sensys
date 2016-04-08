#!/usr/bin/python
#
# Copyright (c) 2015  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
""" Script to execute Gtest Tests """

import sys
from subprocess import Popen, PIPE
import re
import os.path
from os import getenv

DEFAULT_CONFIG_FILE_NAME = "orcm-default-config.xml"
TEST_FILE_NAME = "snmp.xml"

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

def prefix_search():
    """ Returns path to installation path """
    prefix = "/usr/local1/"
    filename = os.getcwd() + "/../../../../../config.status"
    pattern = re.compile(r'S\[\"prefix\"\]=\"(.*)\"')
    with open(filename) as opened_file:
        for line in opened_file:
            match = pattern.search(line)
            if match:
                prefix = match.group(1)
    return prefix

def setup_tests():
    """ Copies test file example to default config file """
    prefix = prefix_search()
    cf_src = getenv("srcdir")
    if cf_src:
        cf_src += "/test_files/" + TEST_FILE_NAME
    else:
        cf_src = "test_files/" + TEST_FILE_NAME
    cf_dst = prefix + "/etc/" + DEFAULT_CONFIG_FILE_NAME
    exists = os.path.isfile(cf_dst)
    if exists:
        os.system("sudo mv %s %s.bak" % (cf_dst, cf_dst))
    os.system("sudo cp %s %s" % (cf_src, cf_dst))
    return cf_dst

def run_tests(gtest_binary):
    """ Execute gtest binary """
    cf_dst = setup_tests()
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
    os.system("sudo mv %s.bak %s" % (cf_dst, cf_dst))
    return ret

if __name__ == '__main__':
    PROG = './snmp_tests'
    sys.exit(run_tests(PROG))
