#!/bin/bash

if test $# -lt 2; then 
	echo "Usage: $(basename $0) file1.val...fileN.val"
	exit 1
fi

xtics="set xtics ("
pos=1
for i in $*;do 
        label=$(basename $i)
	xtics="$xtics '$label' $pos,"
	pos=$(($pos+1))
done;

xtics=$(echo $xtics|sed 's/,$/)/')
paste $* > gnuplot.data

cat > gnuplot.cmd << EOF
set style fill solid 0.25 border -1
set style boxplot outliers pointtype 7
set style data boxplot

set title 'Bench' font 'Arial,14';
$xtics
plot for [i=1:$pos] 'gnuplot.data' using (i):i notitle

EOF

echo "Use gnuplot -p gnuplot.cmd to visualize the result"
