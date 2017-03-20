#!/bin/sh

#  test.sh
#  
#
#  Created by Yang on 2017. 3. 20..
#

#git pull
gcc -I/opt/papi/5.5.1/include -lpthread -O3 main.c /opt/papi/5.5.1/lib/libpapi.a -o main
I_NUM=10000000
for T_NUM in 2 4 8 16 32
do
    printf '\n==============\nT_NUM = %d\n==============\n' $T_NUM
    ./main --i $I_NUM --t $T_NUM
    printf '\n'
done
