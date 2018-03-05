# Chandy-Lamport-snapshot-algorithm

Implemented a distributed banking application using the Chandy-Lamport snapshot algorithm along with Google’s protocol buffer for marshalling-unmarshalling messages and TCP sockets for sending-receiving messages in C++. The distributed bank has multiple branches and very branch knows about all other branches. Application allows to take snapshot at any given time. 

Compilation and execution: 

1. To compile code: Extract all files and folders to a directory. 
2. Set PATH and PKG_CONFIG_PATH to generate cpp code from bank.proto with "protoc --cpp_out=./ bank.proto". 
	PATH=protobuf installed path
	PKG_CONFIG_PATH=protobuf config path
3. This should generate .pb files. Run "make". 
4. Once binary file has been created run branch with bash script as "bash branch.sh <branch_name> <port#>"
5. Create branches.txt file make sure you have content matching with branch instances before running controller. 
6. Run controller with bash script as "bash controller.sh <amount#> branches.txt"
7. Open new window so controller logs will be easy to read.
