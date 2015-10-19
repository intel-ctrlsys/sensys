#!/bin/bash
#
# Return value of script is return value of test_ft_tester
#
# 0 PASS
# 77 not run (problem with test, not with tested content)
# 99 FAIL
#

XML_FILE="--omca cfgi_base_config_file $srcdir/daemon.xml"
SENSORS="--omca sensor ft_tester,heartbeat"
PROB="--omca sensor_ft_tester_daemon_fail_prob 0.0"

./test_ft_tester $XML_FILE $SENSORS $PROB


