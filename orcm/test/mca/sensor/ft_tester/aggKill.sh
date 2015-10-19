#!/bin/bash
#
# Return value of script is return value of test_ft_tester
#
# 0 PASS
# 77 no run (problem with test, not with tested content)
# 99 FAIL
#

XML_FILE="--omca cfgi_base_config_file $srcdir/aggregator.xml"
SENSORS="--omca sensor ft_tester,heartbeat"
PROB="--omca sensor_ft_tester_aggregator_fail_prob 0.05"

./test_ft_tester $XML_FILE $SENSORS $PROB
