#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

cd /sys/fs/resctrl

grep -q mbm_total_bytes info/L3_MON/mon_features || {
	echo "MBM required"
	exit 4
}

which perf > /dev/null || {
	echo "perf tool required"
	exit 4
}

num_rmids=$(cat info/L3_MON/num_rmids)

count=0

result=0

# use as many RMIDs as possible, up to the number of RMIDs
for i in `seq $num_rmids`; do
	mkdir mon_groups/_test_m$((count+1)) 2> /dev/null || break
	if [[ -d mon_groups/_test_m$((count+1))/mon_data ]]; then
		count=$((count+1))
	else
		break;
	fi
done

echo "Created $count monitoring groups."

if [[ $count -eq 0 ]]; then
	echo "need monitoring groups to continue."
	exit 4
fi

declare -a bytes_array

unavailable=0
unavailable0=0

for i in `seq $count`; do
	bytes_array[$i]=$(cat mon_groups/_test_m${i}/mon_data/mon_L3_00/mbm_total_bytes)

	if [[ "${bytes_array[$i]}" = "Unavailable" ]]; then
		unavailable0=$((unavailable0 + 1))
	fi
done

for i in `seq $count`; do
	echo $$ > mon_groups/_test_m${i}/tasks
	taskset 0x1 perf bench mem memcpy -s 100MB -f default > /dev/null
done
echo $$ > tasks

# zero non-integer values
declare -i bytes bytes0

success_count=0

for i in `seq $count`; do
	raw_bytes=$(cat mon_groups/_test_m${i}/mon_data/mon_L3_00/mbm_total_bytes)
	raw_bytes0=${bytes_array[$i]}

	# coerce the value to an integer for math
	bytes=$raw_bytes
	bytes0=$raw_bytes0

	echo -n "g${i}: mbm_total_bytes: $raw_bytes0 -> $raw_bytes"

	if [[ "$raw_bytes" = "Unavailable" ]]; then
		unavailable=$((unavailable + 1))
	fi

	if [[ $bytes -gt $bytes0 ]]; then
		success_count=$((success_count+1))
		echo ""
	else
		echo " (FAIL)"
		result=1
	fi
done

first=$((count-unavailable0))
second=$((count-unavailable))
echo "$count groups, $first returned counts in first pass, $second in second"
echo "successfully measured bandwidth from ${success_count}/${count} groups"

rmdir mon_groups/_test_m*

exit $result
