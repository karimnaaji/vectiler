#!/bin/bash

cd build
for x in {19293..19313}
do
    for y in {24600..24643}
    do
        ./objexport --tilex $x --tiley $y --tilez 16 --sizehint 128 --nsamples 32
        if [ $? -eq 0 ]; then
            echo "[$x $y]: OK" >> report.txt
        else
            echo "[$x $y]: KO" >> report.txt
        fi
    done
done
cd ..
