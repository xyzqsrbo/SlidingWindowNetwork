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

using namespace std;

struct packet {
   
   int seq_num;
   int data_size;
   char data;
};

struct ack {
    int seq_num;
    bool nak;
};

mutex mtx;

condition_variable seq_alert;
condition_variable ack_alert;
packet incoming;
ack ack;

bool ack_flag = false;


int write_into_buffer(packet window[], char buffer[],int* buffer_index,int shift_index);

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
    const int seq_range = 8;
    char buffer[500];
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
    int file_size = 50;
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
    
    
    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
              perror("ERROR on binding");



thread first(listen_for_packets, window, recv_window, socketfd);

int z = 0;
while(z != 1){

    start = 0;
    end = window_size - 1;

    while(buffer_index < sizeof(buffer)){
        shift_index = 0;
        printf("%s", buffer);

    while(!ack_flag){};




    unique_lock<mutex> lck(mtx);

    place_into_window(window, recv_window, seq_range, start, end, ack.seq_num);

    cout << ack.seq_num << "Bro" << endl;

    ack.seq_num = htonl(ack.seq_num);

    

    sendto(socketfd, (struct ack*)&ack, sizeof(ack), 0, 
                                (const struct sockaddr *) &client_addr, sizeof(client_addr));


    

    // if return value of slidingcheck is 0, skip check, write, and shift
    shift_index = slidingCheck(recv_window, window_size);

    check(&start,&end,shift_index, seq_range);

    write_into_buffer(window,buffer, &buffer_index, shift_index);

    shiftWindow(recv_window, window, shift_index, window_size);

    


    ack_flag = false;
    seq_alert.notify_all();
    
    

    }
    z++;
    

}
MyFile.close();
    return 0;
}
// check if buffer is full, if so, write into file, and memset buffer, and also reset buffer_index
int write_into_buffer(packet window[], char buffer[], int* buffer_index,int shift_index) {
    int i = 0;
    cout << "shift_index: " << shift_index << endl;
     while(i != shift_index){
        buffer[*buffer_index] = window[i].data;
        i++;
        cout <<" data: " << buffer[*buffer_index] << endl;
        *buffer_index = *buffer_index + 1;
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

    while(1) {


     recvfrom(socketfd,(struct packet*)&incoming,sizeof(incoming),0, NULL, NULL);



     cout << "boogie woogie " << incoming.data << endl;







    unique_lock<mutex> lck(mtx);
     ack.seq_num = ntohl(incoming.seq_num);

     ack.nak = false;




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
