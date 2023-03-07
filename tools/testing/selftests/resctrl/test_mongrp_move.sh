#!/bin/sh

set -e

rc=0

cleanup()
{
	rmdir _test_*
	rmdir mon_groups/_test_*
}

skip_all()
{
	echo Bail out! $1

	cleanup

	# SKIP code is 4.
	exit 4
}

expect_success()
{
	if [ "$1" -eq 0 ]; then
		echo ok $2
	else
		echo not ok $2
		rc=1
	fi
}

expect_fail()
{
	if [ "$1" -eq 0 ]; then
		echo not ok $2
		rc=1
	else
		echo ok $2
	fi
}

if [ "$(id -u)" != 0 ]; then
	skip_all "must be run as root"
fi

if [ ! -d /sys/fs/resctrl/info ]; then
	mount -t resctrl resctrl /sys/fs/resctrl || skip_all "no resctrlfs"
fi

cd /sys/fs/resctrl

if [ ! -f info/L3_MON/mon_features ]; then
	skip_all "no monitoring support"
fi

if [ ! -f schemata ]; then
	skip_all "no allocation support"
fi

echo "1..11"

if [ -d _test_c1 ] || [ -d _test_c2 ] || [ -d mon_groups/_test_m1 ]; then
	skip_all "test directories already exist"
fi

mkdir _test_c1
mkdir _test_c2
mkdir _test_c3

mkdir mon_groups/_test_m1
echo 1 > mon_groups/_test_m1/cpus

mkdir _test_c1/mon_groups/_test_m1
echo $$ > _test_c1/tasks
echo $$ > _test_c1/mon_groups/_test_m1/tasks

if mv _test_c1/mon_groups/_test_m1 _test_c2/mon_groups; then
	echo "ok 1 # MON group move to new parent succeeded"
else
	echo "1..0 # skip because MON group move to new parent not supported"
	cleanup
	exit 4
fi

set +e

grep -q $$ _test_c2/tasks
expect_success $? "2 # PID in new CTRL_MON group"

grep -q $$ _test_c2/mon_groups/_test_m1/tasks
expect_success $? "3 # PID remains in MON group after move"

grep -q $$ _test_c1/tasks
expect_fail $? "4 # PID no longer in previous CTRL_MON group"

mv _test_c2/mon_groups/_test_m1/cpus mon_groups
expect_fail $? "5 # moving files not allowed"

mv _test_c2/mon_groups/_test_m1 _test_c2/mon_groups/_test_m2
expect_success $? "6 # simple MON directory rename"

mv _test_c2/mon_groups/_test_m2 info
expect_fail $? "7 # move to info not allowed"

mv _test_c2/mon_groups/_test_m2 _test_c2/mon_groups/mon_groups
expect_fail $? "8 # rename to mon_groups not allowed"

mv mon_groups/_test_m1 _test_c1/mon_groups
expect_fail $? "9 # cannot move groups monitoring CPUs"

mv mon_groups/_test_m1 mon_groups/_test_m2
expect_success $? "10 # groups monitoring CPUs can be renamed"

mv mon_groups/_test_m2/mon_data _test_c1/mon_groups
expect_fail $? "11 # cannot move subdirectories of a mon_group"

cleanup

exit $rc
