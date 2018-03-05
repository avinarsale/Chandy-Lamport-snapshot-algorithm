# See README.txt.

.PHONY: all cpp clean

all: cpp 

cpp:    branch    controller

clean:
	rm -f branch controller 
	rm -f protoc_middleman bank.pb.cc bank.pb.h bank_pb2.py com/example/tutorial/bankProtos.java

branch: branch.cc
	pkg-config --cflags protobuf  # fails if protobuf is not installed
	c++ branch.cc bank.pb.cc -o branch `pkg-config --cflags --libs protobuf` -std=c++11

controller: controller.cc
	pkg-config --cflags protobuf  # fails if protobuf is not installed
	c++ controller.cc bank.pb.cc -o controller `pkg-config --cflags --libs protobuf` -std=c++11
