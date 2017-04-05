#!/bin/bash

if test $# -ne 3; then
	echo "Usage: $(basename $0) threadsmax runs duration"
	exit 1
fi

threadsmax=$1
runs=$2
duration=$3

BENCH=$(find -name 'bench-*' -executable)

for bench in $BENCH;do
	rm -f /tmp/$bench.bench
	rm -f /tmp/$bench.time
	rm -f /tmp/$bench.print
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
		
