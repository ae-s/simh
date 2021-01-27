#!/bin/sh

for T in 1 2 3 4 9 12 13 14
do
    echo -n "$T: "
    ./diagnostic-interpreter "$T" > test-"$T".out
    ret=$?
    if [ $ret -eq 0 ]
    then
        echo "OK"
    else
        echo "Fail:"
        grep TROUBLE test-"$T".out
    fi
done

    
