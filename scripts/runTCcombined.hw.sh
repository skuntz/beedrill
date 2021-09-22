#!/bin/bash

# Application specific parameters
base="triangle_"
APPS="count"

G="1" # 20 21 22 23 24 25 26 27 28 29" #single node up to 21 
trials="1"

# HW Counters
get_counters=1
init="emu_diagnostic_tool --write_64b_control_register 0 2000c8 0"
start="emu_diagnostic_tool --write_64b_control_register 0 2000c8 1"
stop="emu_diagnostic_tool --write_64b_control_register 0 2000c8 2"
stalls="emu_diagnostic_tool --read_64b_control_reg_block 0 280030 4"
stall_ticks="emu_diagnostic_tool --read_64b_control_reg_block 0 280050 4"
srio_req="emu_diagnostic_tool --read_64b_control_reg_block 0 280160 3"
srio_rsp="emu_diagnostic_tool --read_64b_control_reg_block 0 280178 3"

    
# Get current configuration
today=$(date +"%Y-%m-%d.%H_%M_%S")
echo $today
ncdimm=$(ls -la /usr/ncdimm/emu_custom_logic*)
echo $ncdimm
driver=$(emu_handler_and_loader --version)
echo $driver
nodes=$(cat /etc/emutechnology/LogicalTotalNodes)
echo $nodes Nodes
clusters=$(cat /etc/emutechnology/num_clusters)
echo $clusters Clusters
gcs=$(cat /etc/emutechnology/num_gcs)
echo $gcs GCs per cluster
echo "Prefetch on/off"
emu_diagnostic_tool --read_64b_control_register 0 200050 
# Is there a way to get the clock?

# Build log directory
launch="emu_multinode_exec 0"
LOGDIR=logs_N${nodes}_${today}
echo

if [ -d $LOGDIR ]; then
    echo "Log dir exists"
else
    echo "Log dir doesn't exist.  Lets create it... "
    mkdir $LOGDIR
    echo "mkdir complete"
fi


for g in $G; do
    for app in $APPS; do
	  

	# ----------- TRIANGLE COUNT --------------------------------------------------------------------
	echo
	outfile=$LOGDIR/$app-G$g-N$nodes
	foo="$launch -- $base$app.mwx --graph_filename inputs/graph500-scale$g --num_trials $trials"
	# TODO: add trials to outfile? 
	time=$(date +"%H_%M_%S")
	echo $time > $outfile.out
	md5=$(md5sum $base$app.mwx)
	echo $md5 >> $outfile.out
	echo $ncdimm >> $outfile.out
	echo $driver >> $outfile.out
	echo $nodes Nodes, $clusters Clusters, $gcs GCs per cluster >> $outfile.out
	echo >> $outfile.out

	# Clear performance coutners
	if [ $get_counters -ne 0 ]; then
	    $init
	    $start
	fi

	# Launch program
	echo $foo
	$foo >> $outfile.out

	# Gather performance counters
	if [ $get_counters -ne 0 ]; then
		$stop
		echo MSP stall
		$stalls
		echo MSP stall time
		$stall_ticks
		echo SRIO REQ
		$srio_req
		echo SRIO RSP
		$srio_rsp
	fi
    done
done
