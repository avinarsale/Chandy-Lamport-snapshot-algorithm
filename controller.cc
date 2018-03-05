#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netdb.h>
#include <sstream>
#include <algorithm>  
#include <ifaddrs.h>
#include <time.h>
#include <ctime>
#include <list>
#include <thread>
#include <chrono> 
#include <random>
#include "bank.pb.h"

using namespace std;

BranchMessage branch_message;
InitBranch branch;

/* 
	Channel states can be zero and when SerializeToString is done protocol buffer for cpp 
	will loose its states to reduce allocation to handle this I used alternative logic 
	where I add dummy $1 into state while sending from branches and remove same after I receive at 
	controller side. Line#90. 
*/
int retrive_snapshots(int snapshotID){
	cout << "\nRetrieving snapshot for snapshot_id:" << snapshotID << "..."<< endl;
	RetrieveSnapshot retriveSnap;
	retriveSnap.set_snapshot_id(snapshotID);
	branch_message.set_allocated_retrieve_snapshot(&retriveSnap);
	string retrive_output;
	if (!branch_message.SerializeToString(&retrive_output)) {
		cerr << "Failed to write branch message." << endl;
		return -1;
	}else{
		for (int i = 0; i < branch.all_branches_size(); i++) {
			InitBranch_Branch branch_all = branch.all_branches(i);

			struct sockaddr_in address;
			int sock = 0, valread;
			struct sockaddr_in serv_addr;
			char buffer[4096] = {0};
			//memset(&serv_addr, '0', sizeof(serv_addr));
			if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
				printf("\nERROR: Socket creation error \n");
				return -1;
			}

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(branch_all.port());
			// Convert IPv4 and IPv6 addresses from text to binary form
			if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
				printf("\nERROR: Invalid address/ Address not supported \n");
				return -1;
			}

			if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
				printf("\nERROR: Connection Failed \n");
				return -1;
			}

			send(sock, retrive_output.c_str(), retrive_output.size(), 0);
			valread=read( sock, buffer, 4096);
			if(valread<0){
				cerr << "ERROR: In reading buffer." << endl;
				return -1;
			}else{
				buffer[valread] = '\0';
				BranchMessage return_bm;
				return_bm.ParseFromString(buffer);
				if(return_bm.has_return_snapshot()){
					ReturnSnapshot returnSnap=return_bm.return_snapshot();
					if(returnSnap.has_local_snapshot()){
						ReturnSnapshot_LocalSnapshot localreturn=returnSnap.local_snapshot();
						cout << "Snapshot of branch " << branch_all.name() << " :"<< endl;
						cout << "Current Balance:" << localreturn.balance() << endl;
						for (int i=0; i < localreturn.channel_state_size(); i++){
							cout << "States of channel#:" << i << " = " << localreturn.channel_state(i)-1 << endl;
						}
					}
				}else{
					cerr << "ERROR: In receiving snapshot_id. " << endl;
					return -1;
				}
			}
			close(sock);
		}
	}
	branch_message.release_retrieve_snapshot();
}
int main(int argc, char* argv[]) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	if (argc != 3) {
		cerr << "Usage: " << argv[0] << "./controller <balance> <branch_list>" << endl;
		return -1;
	}
	ifstream infile(argv[2]);
	string line;
	int number_of_branches=0;
	while (getline(infile, line))
	{
		number_of_branches++;
		istringstream iss(line);
		string in_branch_name, in_ip, in_port;
		if (!(iss >> in_branch_name >> in_ip >> in_port)) { break; }
		InitBranch_Branch* branch_input=branch.add_all_branches();
		branch_input->set_name(in_branch_name);
		branch_input->set_ip(in_ip);
		branch_input->set_port(stoi(in_port));
	}
	infile.close();
	
	if(number_of_branches>0){
		branch.set_balance(atoi(argv[1])/number_of_branches);
	}else{
		cerr << "ERROR: No Branch exist, cannot split balance. " << endl;
		return -1;
	}

	string output;
	branch_message.set_allocated_init_branch(&branch);
	if (!branch_message.SerializeToString(&output)) {
		cerr << "ERROR: Failed to write branch message." << endl;
		return -1;
	}

	cout << "Initial balance for all Branches: " << branch.balance() << endl;
	int i = 4;
	for (int i = 0; i < branch.all_branches_size(); i++) {
		InitBranch_Branch branch_all = branch.all_branches(i);
		
		struct sockaddr_in address;
		int sock = 0, valread;
		struct sockaddr_in serv_addr;
		char buffer[1024] = {0};
		//memset(&serv_addr, '0', sizeof(serv_addr));
		if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			printf("\nERROR: Socket creation error \n");
			return -1;
		}
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(branch_all.port());
		// Convert IPv4 and IPv6 addresses from text to binary form
		if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
			printf("\nERROR: Invalid address/ Address not supported \n");
			return -1;
		}
	  
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
			printf("\nERROR: Connection Failed \n");
			return -1;
		}

		send(sock, output.c_str() , output.size(), 0);
		cout << "Branch: " << branch_all.name() << " added successfully. " << endl;
		close(sock);
		branch_all.clear_name();
		branch_all.clear_ip();
		branch_all.clear_port();
	}
	branch_message.release_init_branch();
	
	//sleep time for initialization
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	
	int snapID=1;
	while(1)
	{
		cout << "Initializing of snapshot(#"<<snapID<< ")" << endl;
		InitSnapshot initSnapshot;
		initSnapshot.set_snapshot_id(snapID);
		branch_message.set_allocated_init_snapshot(&initSnapshot);
		string insnap_output;
		if (!branch_message.SerializeToString(&insnap_output)) {
			cerr << "Failed to write branch message." << endl;
			return -1;
		}else{
			std::random_device seeder;
			std::mt19937 engine(seeder());
			std::uniform_int_distribution<int> dist(0,branch.all_branches_size()-1);
			int random_branch_id = dist(engine);
			InitBranch_Branch branch_all = branch.all_branches(random_branch_id);
			cout << "Selected random branch: " << branch_all.name() << " for initialization of snapshot" << endl;

			struct sockaddr_in address;
			int sock = 0, valread;
			struct sockaddr_in serv_addr;
			char buffer[1024] = {0};
			//memset(&serv_addr, '0', sizeof(serv_addr));
			if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
					printf("\nERROR: Socket creation error \n");
					return -1;
			}

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(branch_all.port());
			// Convert IPv4 and IPv6 addresses from text to binary form
			if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
				printf("\nERROR: Invalid address/ Address not supported \n");
				return -1;
			}

			if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
				printf("\nERROR: Connection Failed \n");
				return -1;
			}

			send(sock, insnap_output.c_str(), insnap_output.size(), 0);
			close(sock);
		}
		branch_message.release_init_snapshot();
		std::this_thread::sleep_for(std::chrono::milliseconds(9000));
		// retrive snapshot 
		retrive_snapshots(snapID);
		snapID++;
		std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	}
	// Optional:  Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
