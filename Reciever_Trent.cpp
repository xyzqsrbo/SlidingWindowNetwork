/******************************************************************************

                              Online C++ Compiler.
               Code, Compile, Run and Debug C++ program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <string>
#include <netdb.h>
#include <array>
#include <fstream>
#include <vector>
using namespace std;

struct packet {
    int ip = 0;
    int data_size;
    int checksum = 0;
    int seq_num;
    int time_sent = 0;
};

struct state {
    int seq_range;
    int file_size;
    int packet_size;
};

struct ack {
    int seq_num;
    bool nak;
};

mutex mtx;

condition_variable seq_alert;
condition_variable ack_alert;
buffer incoming[64000];
ack ack;


bool ack_flag = false;


int write_into_buffer(packet window[] , ostream& MyFile, int shift_index);

int slidingCheck(bool recv_window[], int size);

int shiftWindow(bool recv_window[], packet window[],  int index, int size);

bool check(int* start, int* end, int shift_index, int seq_range);

int listen_for_packets(packet window[], bool recv_window[], int socketfd);

bool place_into_window(packet window[], bool recv_window[], int seq_range, int start, int end, int seq_num);




int main(int argc, char *argv[])
{
    int socketfd, port;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    const int window_size = 4;
    int seq_range = 0;
    int packet_size = 0;
    
    packet window[window_size];
    bool recv_window[window_size];
    int start;
    int end;
    int current_packet = 0;
    int buffer_index = 0;
    int shift_index = window_size;
    int begin = 0;
    int current_seq = 0;
    int i = 0;
    int file_size = 0;
    int data_written = 0;
	
    ofstream MyFile("stupid.txt");
    
    socklen_t length;
    
    socketfd =  socket(AF_INET, SOCK_DGRAM, 0);
     if (socketfd < 0) 
        perror("ERROR opening socket");
        
    memset(&server_addr, 0, sizeof(server_addr)); 
    
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1070);

    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(1065);
    state setup;
    int check = htonl(1);

    
    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
              perror("ERROR on binding");

    cout << "Waiting for initial packet" << endl;
    recvfrom(socketfd,(struct state*)&setup,sizeof(setup),0, NULL, NULL);
    seq_range = ntohl(setup.seq_range);
    file_size = ntohl(setup.file_size);
    packet_size = ntohl(setup.packet_size);

    char incoming_buffer[packet_size + struct_size(window[0])];

    cout << "initial packet recieved" << endl;
    cout << " Seq_range: " << seq_range << "file_size: " << file_size << "packet_size: " << packet_size << endl;
    char buffer[window_size][packet_size + struct_size(window[0])];
     sendto(socketfd, &check, sizeof(check), 0, 
                                (const struct sockaddr *) &client_addr, sizeof(client_addr));




thread first(listen_for_packets, window, recv_window, socketfd);

int j = 0;
while(data_written < file_size){

    start = 0;
    end = window_size - 1;

    while(j != window_size){
        // update sliding window method use here - 
        shift_index = 0;
        printf("%s", buffer);

    while(!ack_flag){};

    findIndex(int start, int end, int seq_num, int seq_range);

    serialize();

    place_into_window(window, recv_window, seq_range, start, end, ack.seq_num);

    read_into_buffer()






    







    unique_lock<mutex> lck(mtx);

   

    cout << ack.seq_num << "Bro" << endl;

    ack.seq_num = htonl(ack.seq_num);

    

    sendto(socketfd, (struct ack*)&ack, sizeof(ack), 0, 
                                (const struct sockaddr *) &client_addr, sizeof(client_addr));


    

    // if return value of slidingcheck is 0, skip check, write, and shift
    shift_index = slidingCheck(recv_window, window_size);

    check(&start,&end,shift_index, seq_range);

    write_into_buffer(window, MyFile, shift_index);

    shiftWindow(recv_window, window, shift_index, window_size);

    


    ack_flag = false;
    seq_alert.notify_all();
    
    

    }
    j++;
    

}
MyFile.close();
    return 0;
}
// check if buffer is full, if so, write into file, and memset buffer, and also reset buffer_index
int write_into_buffer(char buffer[],int buffer_size, int packet_size) {
    int i = 0;
    cout << "shift_index: " << shift_index << endl;
     while(i != buffer_size){
         while(j != packet_size) {
             MyFile.write(buffer, sizeof(window[i].data.at(j)));
             j++;
         }
        i++;
    } 
    return 1 ;
}

// for this method, check return. if its negative or if its greater than end, its a past value
int findIndex(int start, int end, int seq_num, int seq_range) {
    if(start < end || seq_num > end) {
            return seq_num - start;
        } else {
           return seq_range - start + seq_num;
        }
}





bool place_into_window(packet window[], bool recv_window[], int seq_range, int start, int end, int seq_num) {
       cout << seq_range << " - " << start << " - " << end << " - " << seq_num << endl;
        if(start < end || seq_num > end ) {
            window[seq_num - start] = incoming;
            recv_window[seq_num - start] = true;
        } else {
            window[seq_range - start + seq_num] = incoming;
            recv_window[seq_range - start + seq_num] = true;
        }
        
    
    return true;
}



int listen_for_packets(packet window[], bool recv_window[], int socketfd){
    cout << "Thread On" << endl;
    while(1) {


     recvfrom(socketfd,incoming,sizeof(incoming),0, NULL, NULL);



     

    cout << "hello world" << endl;

    







    unique_lock<mutex> lck(mtx);




        ack_flag = true;
       seq_alert.wait(lck);
       

    }


}


int slidingCheck(bool recv_window[], int size) {
    bool pass = false;
    int index = 0; 
    for(int i = 0; i < size; i++) {
        pass = recv_window[i];
        if(!pass) break;
        index++;
    }
    return index;
}

bool check(int* start, int* end, int shift_index, int seq_range){

    *start = *start + shift_index;
    *end = *end + shift_index;

    if(*start > (seq_range - 1 )) {
        *start = *start - seq_range;
    }

     if(*end > (seq_range - 1)){
         *end = *end -seq_range;
     }



    
}

 int shiftWindow(bool recv_window[], packet window[],  int index, int size){
    if(index == 0) return -1;
    
    for(int i =0 ; i < size - index; i++){
        recv_window[i] = recv_window[i+index];
        recv_window[i+index] = false;
        window[i] = window[i+index];
        window[i+index] = {};
    }
    return 0;

}




int struct_size(packet packet) {
    int size = 0;
    int += sizeof(packet.ip);
    int += sizeof(packet.data_size);
    int += sizeof(packet.checksum);
    int += sizeof(packet.seq_num);
    int += sizeof(packet.time_sent);
    return size;
}

int struct_size(state packet) {
    int size = 0;
    int += sizeof(packet.seq_range);
    int += sizeof(packet.file_size);
    int += sizeof(packet.packet_size);
    return size;
}

int file_size(ifstream& file){
    int file_size = 0;
    file.seekg(0, ios_base::end);
    file_size = file.tellg();
    file.clear();
    file.seekg(0);
    return file_size;
}

int write_into_file(ifstream& file, char buffer[][], int packet_size, int window_size, int file_size){
    int i =0;
    while(i != window_size) {
        file.write(buffer[i], packet_size);
        if(file.tellg() >= file_size) {
            break;
        }
        i++;
    }

    return i;
}

int serialize(char buffer[], packet window, int buffer_size, int packet_size) {
    
    memcpy(&window.seq_num,buffer + packet_size , sizeof(window.seq_num));
    memcpy(&window.data_size, buffer + packet_size + sizeof(window.seq_num), sizeof(window.data_size));
    return 0;
}
bool update_sliding_window(packet window[], int seq_range, int* current_seq, int shift_index, int window_size, int packet_size) {
    int i = window_size - shift_index;

    while(i != window_size) {
        window[i].seq_num = htonl(*current_seq);
        window[i].data_size = htonl(packet_size);

        if(*current_seq >= seq_range) {
            current_seq = 0;
        }
        *current_packet += 1;
        i++;
    }
     
bool     
    return true;
}