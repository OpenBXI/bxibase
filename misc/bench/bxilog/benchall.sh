#!/bin/bash

if test $# -ne 3; then
	echo "Usage: $(basename $0) threadsmax runs duration"
	exit 1
fi

threadsmax=$1
runs=$2
duration=$3

BENCH="zlog_bench bxilog_bench $(echo *.py)"

for bench in $BENCH;do
	rm -f /tmp/$bench.bench
	rm -f /tmp/$bench.time
done

for thread in $(seq 1 $threadsmax);do
	for run in $(seq 1 $runs); do
		for bench in $BENCH;do
			cmd="./run.sh $bench $thread $duration"
			echo "Benchmark-$thread.$run/$threadsmax.$runs: $cmd"
			$cmd
		done
	done
done
		
