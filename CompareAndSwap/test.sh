#!/bin/sh

#  test.sh
#  
#
#  Created by Yang on 2017. 3. 20..
#

git pull
gcc -I/opt/papi/5.5.1/include -lpthread -O3 main.c /opt/papi/5.5.1/lib/libpapi.a -o main
./main --i 1000000 --t 2
./main --i 1000000 --t 4
./main --i 1000000 --t 8
./main --i 1000000 --t 16
>>>>>>> ba2b2d40e7f3683fec5d8d0d344d235019bb4d3c
