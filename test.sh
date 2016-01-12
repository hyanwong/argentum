#!/bin/bash

set -o nounset
set -o errexit
set -o pipefail

i=1
while true
do
    ./rand-matrix $i 20 20 > rand.input
    ./main -S -i rand.input -v --debug --no-prediction --enumerate > /dev/null
    i=$((i+1))
done
