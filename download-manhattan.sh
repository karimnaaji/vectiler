#!/bin/bash

offsetx=0

cd build
#for x in {19293..19313}
for x in {19293..19298}
do
    offsetx=$((offsetx+2))
    offsety=0
    #for y in {24600..24643}
    for y in {24639..24643}
    do
        offsety=$((offsety-2))
        #./objexport --tilex $x --tiley $y --tilez 16 --sizehint 128 --nsamples 32
        ./objexport --name manhattan --tilex $x --tiley $y --tilez 16 --offsetx $offsetx --offsety $offsety --append 1
        if [ $? -eq 0 ]; then
            echo "[$x $y]: OK" >> report.txt
        else
            echo "[$x $y]: KO" >> report.txt
        fi
    done
done
cd ..
