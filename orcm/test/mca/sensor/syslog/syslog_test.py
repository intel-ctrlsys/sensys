#!/usr/bin/python

#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#


import sys
import subprocess
from subprocess import Popen, PIPE, STDOUT

def get_gtests(gtest_binary):
    process = Popen([ gtest_binary, '--gtest_list_tests' ], stdout=PIPE)
    (output, err) = process.communicate()
    exit_code = process.wait()
    if exit_code != 0:
        return []
    lines=output.split('\n')
    fixture=''
    c=0
    tests=[]
    for line in lines:
        c=c+1
        if c!=1:
            if line == '':
                continue
            if line[0] != ' ':
                fixture=line
            else:
                test=fixture+line[2:]
                tests.append(test)
    return tests

def execute_test(gtest_binary, test_name):
    process = Popen([ gtest_binary, "--gtest_color=yes", "--gtest_filter=%s"%(test_name) ], stdout=PIPE, stderr=PIPE)
    (output, err) = process.communicate()
    rv=process.wait()
    return (rv, output, err)

def run_tests(gtest_binary):
    tests=get_gtests(gtest_binary)
    ret = 0
    passed=0
    failed=0
    print '+RUNNING: %s'%gtest_binary
    for test in tests:
        (rv,output, err)=execute_test(gtest_binary, test)
        if rv != 0:
            print '+STDERR:\n%s'%err
            print '+STDOUT:\n%s'%output
            ret=rv
            failed=failed+1
        else:
            passed=passed+1
    print '+TEST SUMMARY PASSED = %d'%(passed)
    print '+TEST SUMMARY FAILED = %d'%(failed)
    return ret

prog='./syslog_tests'
sys.exit(run_tests(prog))
