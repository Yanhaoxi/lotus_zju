#!/bin/bash
# Example script for running approximation benchmarks

#echo "alphabet ab trees..."
##path="lossy_benchmarks/student-cfg/alphabet-ab/student-"
##for i in {1..1346}; do 
##    ./newton --lossy --file ${path}${i}.g >> "ab-trees".log;
##done
#for filename in lossy_benchmarks/student-cfg/alphabet-ab/*; do ./newton --lossy --file ${filename} >> "ab-trees".log; done

#echo "alphabet abc trees..."
##path="lossy_benchmarks/student-cfg/alphabet-abc/student-"
##for i in {1..103}; do 
##    ./newton --lossy --file ${path}${i}.g >> "abc-trees".log;
##done
#for filename in lossy_benchmarks/student-cfg/alphabet-abc/*; do ./newton --lossy --file ${filename} >> "abc-trees".log; done

#echo "alphabet 01 trees..."
##path="lossy_benchmarks/student-cfg/alphabet-01/student-"
##for i in {1..350}; do 
##    ./newton --lossy --file ${path}${i}.g >> "01-trees".log;
##done
#for filename in lossy_benchmarks/student-cfg/alphabet-01/*; do ./newton --lossy --file ${filename} >> "01-trees".log; done

#echo "alphabet a trees..."
##path="lossy_benchmarks/student-cfg/alphabet-a/student-"
##for i in {1..93}; do 
##    ./newton --lossy --file ${path}${i}.g >> "a-trees".log;
##done
#for filename in lossy_benchmarks/student-cfg/alphabet-a/*; do ./newton --lossy --file ${filename} >> "a-trees".log; done

for filename in lossy_benchmarks/student-cfg/alphabet-a/*; do echo -n ${filename}; ./newton --lossyC --file ${filename}; done

