#!/bin/sh

#  test.sh
#  
#
#  Created by Yang on 2017. 3. 20..
#

git pull
gcc -I/opt/papi/5.5.1/include -lpthread -O3 main.c /opt/papi/5.5.1/lib/libpapi.a -o main
I_NUM=10000000
T_NUM=2
while [ $T_NUM -lt 17 ]
do
    ./main --i $I --t $T_NUM
    echo '\n\n'
    $T_NUM=$T_NUM-1
done
