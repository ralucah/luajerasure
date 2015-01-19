#!/bin/bash

#test1; keep K=10, M=3 and vary W(8,16 or 32) and block size (16KB ... 16MB)

#K=10
#M=3
#FILES=`ls -Sr testfiles`
#for W in 8 16 32
#do
#    for FILE in $FILES
#    do
#        OUT="results/$K-$M-$W-$FILE"
#        for ITER in $(seq 1 100)
#        do
#            lua luajerasure_test.lua $K $M $W testfiles/$FILE >> $OUT
#        done
#    done
#done

#test2: encode + decode
K=10
M=3
W=8
FILE="16MB"
for ITER in $(seq 1 100)
do
    lua luajerasure_test.lua $K $M $W testfiles/$FILE >> decode_10_3_8_16MB
done
