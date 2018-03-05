#!/bin/bash +vx
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/home/yaoliu/src_code/local/lib"
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/home/phao3/protobuf/bin/lib"
./branch $1 $2
