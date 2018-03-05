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

#define INIT_BALANCE 200

using namespace std;
std::mutex myMutex;
BranchMessage branch_main;
InitBranch curr_branch;
string curr_branch_name;
string curr_branch_ip;
int curr_branch_port;
bool is_branch_initialized = false;

// for snapshots checking
int markers_sent = 0;
int markers_received = 0;

//snapdhot id and its global state
map <int, ReturnSnapshot_LocalSnapshot> curr_snapshots;

// DataStruct for marker recording (to open recording on a incoming channel)
//map<int,>
map <string, bool> marker_record_tracker;
map <string, int> incoming_channels;

void update_balance(uint32_t reducing,bool isAdd){
	if(isAdd){
		curr_branch.set_balance(curr_branch.balance()+reducing);
	}else{
		curr_branch.set_balance(curr_branch.balance()-reducing);
	}
}

int transfer_money_schedule()
{
	//cout << curr_branch_port << "::" << " transfer_money_schedule Thread started for random transfers." << endl;
	while(1)
	{
		while(!is_branch_initialized){}
		std::random_device seeder;
		std::mt19937 engine(seeder());
		std::uniform_int_distribution<int> dist(1000,5000);
		int random_sleep_time = dist(engine);
		std::this_thread::sleep_for(std::chrono::milliseconds(random_sleep_time)); // sleep for random time
		
		std::random_device seeder_3;
		std::mt19937 engine_3(seeder());
		std::uniform_int_distribution<int> random_per(1,5);
		int random_percentage = random_per(engine);	
		uint32_t sending_money=(curr_branch.balance()*random_percentage)/100;
		BranchMessage branch_message_scheduler;
		Transfer transfer_money;
		transfer_money.set_money(sending_money);
		branch_message_scheduler.set_allocated_transfer(&transfer_money);
		string trans_output;
		if (!branch_message_scheduler.SerializeToString(&trans_output)) {
			cerr << "ERROR: Failed to write branch message." << endl;
			return -1;
		}else{
			std::random_device seeder_2;
			std::mt19937 engine_2(seeder());
			std::uniform_int_distribution<int> random_branch(0,curr_branch.all_branches_size()-1);
			int random_branch_id = random_branch(engine);
			InitBranch_Branch branch_all = curr_branch.all_branches(random_branch_id);			
			if(curr_branch_port==branch_all.port()){
				//Skip
			}else{
				uint32_t old_balance=curr_branch.balance();
				//cout << curr_branch_port << "::" <<  " transfer_money_schedule Current balance: " << curr_branch.balance() << " random_percentage:" << random_percentage << ":PER:" << sending_money << endl;
				if(old_balance>sending_money){
					myMutex.lock();
					update_balance(sending_money,false);
					myMutex.unlock();
				}else{
					cerr << "ERROR: Branch balance cannot got to negative.. cancelling transaction. " << endl;
					return -1;
				}
				struct sockaddr_in address;
				int sock = 0, valread;
				struct sockaddr_in serv_addr;
				memset(&serv_addr, '0', sizeof(serv_addr));
				if ((sock = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
					cerr << "ERROR: Socket creation error. " << endl;
					return -1;
				}
				serv_addr.sin_family = AF_INET;
				serv_addr.sin_port = htons(branch_all.port());
				if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
					cerr << "ERROR: Invalid address or address not supported. " << endl;
					return -1;
				}
				if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
					cerr << curr_branch_port << "ERROR: Connection Failed. " << endl;
					return -1;
				}
				if(send(sock, trans_output.c_str(), trans_output.size(), 0)!=trans_output.size()){ // send BranchMessage
					cerr << "1.1.1 ERROR: In sending socket_buffer." << endl;
					exit(EXIT_FAILURE);
				}
				char buffer_b_name[512];
				strcpy(buffer_b_name, curr_branch_name.c_str());
				if(send(sock, buffer_b_name, strlen(buffer_b_name), 0)!=strlen(buffer_b_name)){ // send branch name
					cerr << "1.1.2 ERROR: In sending socket_buffer." << endl;
					exit(EXIT_FAILURE);
				}
				//cout << curr_branch_port << "::" << " transfer_money_schedule SENT successfully to branch: " << branch_all.port() << ":balance:::" << curr_branch.balance() << endl;
				close(sock);
			}
		}
		branch_message_scheduler.release_transfer();
	}
	return 0;
}

void init_snapshot(int snapshot_id_in){
	BranchMessage branch_message_init_snapshot;
	//cout << curr_branch_port << "::" << " init_snapshot snapshot initialization for snapshot_id: " << snapshot_id_in << endl;
	
	//cout << curr_branch_port << "::" << "recording balance as:" << curr_branch.balance() << endl; 
	
	ReturnSnapshot_LocalSnapshot localSnapshot;
	localSnapshot.set_snapshot_id(snapshot_id_in);
	localSnapshot.set_balance(curr_branch.balance());
	localSnapshot.clear_channel_state();
	
	// turn on recording of any messages from branch branch_all.name()
	for (int i = 0; i < curr_branch.all_branches_size(); i++) {
		InitBranch_Branch one_branch = curr_branch.all_branches(i);
		if(curr_branch_port!=one_branch.port()){
			auto it2=marker_record_tracker.find(one_branch.name());
			if(it2 != marker_record_tracker.end()){
				//cout << curr_branch_port << "::" << "init_snapshot Turning ON recording on:" << one_branch.name() << endl;
				it2->second=true;
			}
		}
	}
	
	for (std::map<string,int>::iterator it=incoming_channels.begin(); it!=incoming_channels.end(); ++it){
		if(it->second > 0){
			//cout << curr_branch_port << ":: init_snapshot clearing it->second..." << endl;
			it->second=0;
		}
	}
	curr_snapshots.insert(pair<int,ReturnSnapshot_LocalSnapshot>(localSnapshot.snapshot_id(), localSnapshot));
	markers_sent = markers_received = 0;
	
	Marker init_marker;
	init_marker.set_snapshot_id(localSnapshot.snapshot_id());
	branch_message_init_snapshot.set_allocated_marker(&init_marker);
	string marker_output;
	if (!branch_message_init_snapshot.SerializeToString(&marker_output)) {
		cerr << "Failed to write branch message." << endl;
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < curr_branch.all_branches_size(); i++) {
		InitBranch_Branch branch_all = curr_branch.all_branches(i);
		if(curr_branch_port!=branch_all.port()){
			//cout << curr_branch_port << "::" << " init_snapshot Sending marker to:" << branch_all.name() << ":port:" << curr_branch.all_branches(i).port() << endl;
			markers_sent++;

			struct sockaddr_in address;
			int sock1 = 0, valread=0;
			struct sockaddr_in serv_addr;
			char buffer[4096] = {0};
			memset(&serv_addr, '0', sizeof(serv_addr));
			if ((sock1 = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
				printf("\n Socket creation error \n");
				exit(EXIT_FAILURE);
			}
			
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(branch_all.port());
			//Convert IPv4 and IPv6 addresses from text to binary form
			if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
				printf("\nInvalid address/ Address not supported \n");
				exit(EXIT_FAILURE);
			}
		  
			if (connect(sock1, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
				printf("\nConnection Failed \n");
				exit(EXIT_FAILURE);
			}

			if(send(sock1, marker_output.c_str(), marker_output.size(), 0) != marker_output.size()){ // send BranchMessage
				cerr << "1.2.2 ERROR: In sending socket_buffer." << endl;
				exit(EXIT_FAILURE);
			}
			char buffer_b_name[512];
			strcpy(buffer_b_name, curr_branch_name.c_str());
			if(send(sock1, buffer_b_name, strlen(buffer_b_name), 0) != strlen(buffer_b_name)){ // send branch name
				cerr << "1.2.1 ERROR: In sending socket_buffer." << endl;
				exit(EXIT_FAILURE);
			}
			//cout << curr_branch_port << "::" << " init_snapshot marker_sent_successfully to: " << branch_all.name() << " from:" << curr_branch_name<< endl;
			close(sock1);
		}
	}
	branch_message_init_snapshot.release_marker();
	//cout << curr_branch_port << "::" << " init_snapshot snapshot initialization DONE: " << snapshot_id_in << endl;
}

void marker(char *branch_name_sender, int snapshot_id_inp){
	//cout << curr_branch_port << "::" << " marker Marker received from branch: " << branch_name_sender << " with id:" << snapshot_id_inp << endl;

	auto it=curr_snapshots.find(snapshot_id_inp);
	if(it != curr_snapshots.end()){ // true = second marker message
		// turn off marker recording from branch_name_sender
		auto it3=marker_record_tracker.find(branch_name_sender);
		if(it3 != marker_record_tracker.end()){
			//cout << curr_branch_port << ": marker Turning OFF recording on:" << branch_name_sender << ":->:"<< curr_branch_name <<":"<< endl;
			it3->second=false;
		}
		for (std::map<string,int>::iterator it1=incoming_channels.begin(); it1!=incoming_channels.end(); ++it1){
			//cout << curr_branch_port << ":marker recording channel state:" << it1->first << endl;
			if(branch_name_sender==it1->first){
				//cout << curr_branch_port << ":: Adding amount: " << it1->second << " from incoming branch:" << it1->first << endl;
				if(it1->second==0){
					it->second.add_channel_state(1);
				}else{
					it->second.add_channel_state(1+it1->second);
				}
			}
		}
		//cout << curr_branch_port << "::" << " marker snapshot_id:" << snapshot_id_inp << " received from: " << branch_name_sender << " for second time. CHANNEL count#:" << it->second.channel_state_size() << endl;
	}
	else // first time marker message is being received
	{
		//cout << curr_branch_port << "::" << " marker snapshot_id:" << snapshot_id_inp << " received from: " << branch_name_sender << " for first time. " << endl; 
		//cout << curr_branch_port << "::" << "recording balance as:" << curr_branch.balance() << endl; 
		BranchMessage branch_message_init_snapshot;
		ReturnSnapshot_LocalSnapshot localSnapshot;
		localSnapshot.set_snapshot_id(snapshot_id_inp);
		localSnapshot.set_balance(curr_branch.balance());
		localSnapshot.clear_channel_state();
		for (std::map<string,int>::iterator it=incoming_channels.begin(); it!=incoming_channels.end(); ++it){
			if(it->second > 0){
				//cout << curr_branch_port << ":: marker clearing it->second..." << endl;
				it->second=0;
			}
		}
		curr_snapshots.insert(pair<int,ReturnSnapshot_LocalSnapshot>(localSnapshot.snapshot_id(), localSnapshot));
		markers_sent = markers_received = 0;

		Marker init_marker;
		init_marker.set_snapshot_id(localSnapshot.snapshot_id());
		branch_message_init_snapshot.set_allocated_marker(&init_marker);
		string marker_output;
		if (!branch_message_init_snapshot.SerializeToString(&marker_output)) {
			cerr << "Failed to write branch message." << endl;
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < curr_branch.all_branches_size(); i++) {
			InitBranch_Branch branch_all = curr_branch.all_branches(i);
			bool is_already_sent;
			if(curr_branch_port!=branch_all.port()){
				//cout << curr_branch_port << "::" << " marker Sending marker to:" << branch_all.port() << endl;
				markers_sent++;

				// turn on recording of any messages from branch branch_all.name()
				auto it2=marker_record_tracker.find(branch_all.name());
				if(it2 != marker_record_tracker.end()){
					if(it2->first!=branch_name_sender){
						//cout << curr_branch_port << ": marker Turning ON recording on:" << it2->first << ":->:"<< curr_branch_name <<":"<< endl;
						it2->second=true;
					}else{
						auto it11=curr_snapshots.find(localSnapshot.snapshot_id());// mark channel as empty
						if(it11 != curr_snapshots.end()){
							it11->second.add_channel_state(1);
						}
					}
				}
				
				struct sockaddr_in address;
				int sock2 = 0, valread = 0;
				struct sockaddr_in serv_addr;
				char buffer[4096] = {0};
				memset(&serv_addr, '0', sizeof(serv_addr));
				if ((sock2 = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
					printf("\n Socket creation error \n");
					exit(EXIT_FAILURE);
				}
				
				serv_addr.sin_family = AF_INET;
				serv_addr.sin_port = htons(branch_all.port());
				//Convert IPv4 and IPv6 addresses from text to binary form
				if(inet_pton(AF_INET, branch_all.ip().c_str(), &serv_addr.sin_addr)<=0) {
					printf("\nInvalid address/ Address not supported \n");
					exit(EXIT_FAILURE);
				}

				if (connect(sock2, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
					printf("\nConnection Failed \n");
					exit(EXIT_FAILURE);
				}

				if(send(sock2, marker_output.c_str(), marker_output.size(), 0) != marker_output.size()){ // send BranchMessage
					cerr << "1.3.2 ERROR: In sending socket_buffer." << endl;
					exit(EXIT_FAILURE);
				}
				char buffer_b_name[512];
				strcpy(buffer_b_name, curr_branch_name.c_str());				
				if(send(sock2, buffer_b_name, strlen(buffer_b_name), 0) != strlen(buffer_b_name)){ // send branch name
					cerr << "1.3.1 ERROR: In sending socket_buffer." << endl;
					exit(EXIT_FAILURE);
				}
				//cout << curr_branch_port << "::" << " marker marker_sent_successfully to: " << branch_all.name() << " from:" << curr_branch_name<< endl;
				close(sock2);
			}
		}
		branch_message_init_snapshot.release_marker();
	}
	markers_received++;
	if(markers_received == markers_sent){
		//cout << curr_branch_port << ":--marker successfully done --:" << markers_received << ":" << markers_sent << endl;
	}
	//cout << curr_branch_port << "::" << " marker Marker received DONE branch: " << branch_name_sender << " with id:" << snapshot_id_inp << endl;
	
} // marker()

// Add money
void add_money(char *branch_name_sender, int amount){
	//cout << curr_branch_port << "::" << " main_Transfer_call, from branch:" << branch_name_sender << " & current balance: " << curr_branch.balance() << endl;
	//cout << curr_branch_port << "::" << " main_Transfer_call, Adding amount: " << amount << endl;
	auto it = marker_record_tracker.find(branch_name_sender);
	if(it!=marker_record_tracker.end()){
		if(it->second){
			//cout << curr_branch_port << "::" << "main_Transfer_call add amount to incoming channel list" << endl;
			auto itr=incoming_channels.find(branch_name_sender);
			if(itr!=incoming_channels.end()){
				//cout << curr_branch_port << ":Incoming " << branch_name_sender << " -> " << curr_branch_name << " Old Money:" << itr->second << endl;
				itr->second=itr->second+amount;
				//cout << curr_branch_port << ":Incoming " << branch_name_sender << " -> " << curr_branch_name << " New Money:" << itr->second << endl;
			}
		}
	}
	myMutex.lock();
	update_balance(amount,true);
	myMutex.unlock();
	//cout << curr_branch_port << "::" << " main_Transfer_call, Updated new balance: " << curr_branch.balance() << endl;
}

int main(int argc, char* argv[]) { 
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	if (argc != 3) {
		cerr << "Usage: " << argv[0] << " ./branch <branch_name> <port_number>" << endl;
		return -1;
	}
	
	curr_branch_name=argv[1];
	curr_branch_port=atoi(argv[2]);
	
	struct sockaddr_in socket_address;
    int server_ds, my_socket, read_count;
    socklen_t address_len = sizeof(socket_address);
    int option=1;
    char socket_buffer[4096] = {0};

    if ((server_ds = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
        cerr << "ERROR: failed in creating socket" << endl;
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_ds, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option))) {
        cerr << "ERROR: failed setsockopt" << endl;
        exit(EXIT_FAILURE);
    }
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = INADDR_ANY;
    socket_address.sin_port = htons(curr_branch_port); //htons(8080);

    if (bind(server_ds, (struct sockaddr *)&socket_address, sizeof(socket_address))<0) {
        cerr << "ERROR: binding failed" << endl;
        exit(EXIT_FAILURE);
    }
    if (listen(server_ds, 20) < 0) {
        cerr << "ERROR: failed at listen" << endl;
        exit(EXIT_FAILURE);
    }
	
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	struct hostent* host_n;
	host_n = gethostbyname(hostname);
	int socket_address_len = sizeof(struct sockaddr);
    //struct sockaddr_in socket_address_1;
	
	struct ifaddrs *addrs, *tmp;
	if (getifaddrs(&addrs) == -1) {
		cerr << "ifaddrs is not set properly.. issue with environment setup!" << endl;
		exit(EXIT_FAILURE);
	}
	tmp = addrs;
	string eth0="eth0";
	while (tmp){
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET){
			struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
			if(eth0.compare(string(tmp->ifa_name))==0){
				curr_branch_ip=inet_ntoa(pAddr->sin_addr);
			}
		}
		tmp = tmp->ifa_next;
	}
	freeifaddrs(addrs);

	cout << "Starting branch.. " << INIT_BALANCE << endl;
	curr_branch.set_balance(INIT_BALANCE);
	cout << "Host name: " << host_n->h_name << endl;
 	cout << "Branch ip: " << curr_branch_ip << " & Branch port: " << curr_branch_port << endl;

	//start thread with scheduled transfer
	thread t1(transfer_money_schedule);
	
	while(1)
	{
		if ((my_socket = accept(server_ds, (struct sockaddr *)&socket_address, (socklen_t*)&address_len))<0) {
			cerr << "ERROR: failed to accept" << endl;
			exit(EXIT_FAILURE);
		}

		read_count=read( my_socket, socket_buffer, 4096);
		if(read_count<0){
			cerr << "ERROR: In reading socket_buffer." << endl;
			return -1;
		}
		socket_buffer[read_count] = '\0';
		//myMutex.lock();
		branch_main.ParseFromString(socket_buffer);
		//myMutex.unlock();
		//cout << curr_branch_port << "::::-----------------------------------sc:" << static_cast<int>(branch_main.ByteSizeLong()) << "-----------------------:"<< branch_main.ByteSizeLong() << "-----rc:" << read_count<< endl;
		
		// initialize branch with new balance and add other branches to list 
		if(branch_main.has_init_branch()){
			curr_branch=branch_main.init_branch();
			is_branch_initialized=true;
			//cout << curr_branch_port << "::" << " main initialized branch with new branch balance: " << curr_branch.balance() << endl;
			for (int i = 0; i < curr_branch.all_branches_size(); i++) {
				const InitBranch_Branch& branch_all = curr_branch.all_branches(i);
				//cout << curr_branch_port << ": main Added to banches' list Branch name: " << branch_all.name();
				//cout << ", Branch IP: " << branch_all.ip() << " & Branch port: " << branch_all.port() << endl;
				incoming_channels.insert(pair<string,bool>(branch_all.name(),0));
				marker_record_tracker.insert(pair<string,bool>(branch_all.name(),false));
			}
		}
		
		// init snapshot
		if(branch_main.has_init_snapshot()){
			init_snapshot(branch_main.init_snapshot().snapshot_id());
			//cout << curr_branch_port << "::" << " main_init_snapshot_Return & balance: " << curr_branch.balance() << endl;
		}
		
		// Receive money
		if(branch_main.has_transfer()){
			char branch_name_sender[30];
			int diff_read=read_count-static_cast<int>(branch_main.ByteSizeLong());
			if(diff_read==0){
				read_count=read( my_socket, socket_buffer, 4096);
				if(read_count<0){
					cerr << "1.0.1 ERROR: In reading socket_buffer." << endl;
					return -1;
				}
				socket_buffer[read_count] = '\0';
				sscanf (socket_buffer, "%s" , branch_name_sender);
			}else{
				memcpy( branch_name_sender, &socket_buffer[branch_main.ByteSizeLong()], diff_read );
				branch_name_sender[diff_read]='\0';
				read_count=read( my_socket, socket_buffer, 4096);
				if(read_count<0){
					cerr << "1.0.1 ERROR: In reading socket_buffer." << endl;
					return -1;
				}
				socket_buffer[read_count] = '\0';
			}
			//myMutex.lock();
			add_money(branch_name_sender,branch_main.transfer().money());
			//myMutex.unlock();
		}
		
		//Receive Marker
		if(branch_main.has_marker()){
			char branch_name_sender[30];
			int diff_read=read_count-static_cast<int>(branch_main.ByteSizeLong());
			if(diff_read==0){
				read_count=read( my_socket, socket_buffer, 4096);
				if(read_count<0){
					cerr << "1.0.1 ERROR: In reading socket_buffer." << endl;
					return -1;
				}
				socket_buffer[read_count] = '\0';
				sscanf (socket_buffer, "%s" , branch_name_sender);
			}else{
				memcpy( branch_name_sender, &socket_buffer[branch_main.ByteSizeLong()], diff_read );
				branch_name_sender[diff_read]='\0';
				read_count=read( my_socket, socket_buffer, 4096);
				if(read_count<0){
					cerr << "1.0.1 ERROR: In reading socket_buffer." << endl;
					return -1;
				}
				socket_buffer[read_count] = '\0';
			}

			//cout << curr_branch_port << "::" << " main_marker_called, from branch:" << branch_name_sender << " to branch: " << curr_branch_name << endl; 
			marker(branch_name_sender,branch_main.marker().snapshot_id());
			//cout << curr_branch_port << "::" << " main_marker_return, from branch:" << branch_name_sender << " to branch: " << curr_branch_name << endl; 
		}
		
		// Retrieve Snapshot
		if(branch_main.has_retrieve_snapshot()){
			//cout << curr_branch_port << "::" << "Retrieve Snapshot called for:" << branch_main.retrieve_snapshot().snapshot_id() << endl;
			BranchMessage return_BranchMessage;
			ReturnSnapshot return_snapshot;
			auto it=curr_snapshots.find(branch_main.retrieve_snapshot().snapshot_id());
			if(it != curr_snapshots.end()){
				return_snapshot.set_allocated_local_snapshot(&it->second);
			}else{
				//cout << curr_branch_port << "::" << "Retrieve Snapshot :::::::::::::::::::::::::::::::::::::::::::::: Wrong!" << endl;
			}
			//cout << curr_branch_port << "::" << "Retrieve Snapshot channel size:" << return_snapshot.local_snapshot().channel_state_size() << endl;
			for (int i=0; i < return_snapshot.local_snapshot().channel_state_size(); i++){
				//cout << "channel#:" << i << ": amount:" << return_snapshot.local_snapshot().channel_state(i) << endl;
			}
			return_BranchMessage.set_allocated_return_snapshot(&return_snapshot);
			string return_output;
			
			//cout << curr_branch_port << "------------------------->ByteSizeLong():" << return_BranchMessage.ByteSizeLong() << endl;
			
			if (!return_BranchMessage.SerializeToString(&return_output)) {
				cerr << "Failed to write branch message." << endl;
				return -1;
			}else{
				//cout << curr_branch_port << "::" << " sending Retrieved Snapshot...." << endl;
				if(send(my_socket , return_output.c_str() , return_output.size() , 0 ) != return_output.size()){ 
					cerr << "1.0 ERROR: In sending socket_buffer." << endl;
					exit(EXIT_FAILURE);
				}else{
					//cout << curr_branch_port << "::" << "Retrieve Snapshot Sent successfully!!" << endl;
				}
			}
			return_snapshot.release_local_snapshot();
			return_BranchMessage.release_return_snapshot();
		}
		close(my_socket);
	} // while(1)

	// Optional:  Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();

	// join thread to schedule transfers of random money to random branch at random time  
	t1.join();
	return 0;
}
