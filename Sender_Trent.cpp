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
#include <chrono>
using namespace std;

mutex mtx;

condition_variable cv;

/* packets for sending message
ip - sender ip address
seq_num - sequence number of data
data_size - size of data
data - data to be sent
sentTime - the time the packet was sent
*/
struct packet {
   int seq_num;
   int data_size;
   char data;
   std::chrono::time_point<std::chrono::steady_clock> startTime;
};


/* struct for recieving acknowledgements from reciever
seq_num sequence number of acknowledged package
nak - bool for problems - if true, package was damaged
*/
struct ack {
int seq_num;
bool nak;
};

/* our sl

*/


struct rec_send {
    bool sent = false;

    bool recieved = false;

};

pair <string, int> recv_data;

bool recv_flag;




bool fill_window(packet window[], char buffer[], int seq_range, int* current_seq, int shift_index, int* buffer_index, int size); 

bool check(int* start, int* end, int shift_index, int seq_range);

int slidingCheck(rec_send recv_window[], int size);

int shiftWindow(rec_send recv_window[], packet window[],  int index, int size);

int print_packet(packet window[], bool all, int index);

void listen_to_ack(int socketfd,rec_send recv_window[]);











int main(int argc, char *argv[])
{
    // Initialize socket variables


    /*
    Basic code to get ip address. Contained within the host structure
    Needs testing done - Not sure if code works or if it actually pulls
    correct IP Address from the current poseidon server
    */
    char ipAddress[1024];
    gethostname(ipAddress, sizeof(ipAddress));
    struct hostent *host = gethostbyname(ipAddress);

    
    // Declaration of variables for our socket
    int socketfd, port;
    struct sockaddr_in server_addr, client_addr;


    memset(&server_addr, 0, sizeof(server_addr)); 
    
    /*
    Client is for the recieving end/destination
    Server is for the sending end/source
    Need Port # and IP Address for both.
    */
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(1070);

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1065);

    
    int buffer_size = 4096;
    char buffer[buffer_size];
    
    
    const int window_size = 4;
    const int seq_range = 8;
    packet window[window_size];
    rec_send recv_window[window_size];
    int start;
    int end;
    int current_packet = 0;
    int buffer_index = 0;
    int shift_index = 4;
    int begin = 0;
    int current_seq = 0;



    FILE * fp = fopen("test.txt", "r");
    socketfd =  socket(AF_INET, SOCK_DGRAM, 0);
     if (socketfd < 0) 
        perror("ERROR opening socket");
    
    
    
    
    if (::bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
              perror("ERROR on binding");

cout << "Does this work" << endl; 
    buffer_size = fread(buffer, 1, sizeof(buffer) + 1, fp);

   
    

    thread first(listen_to_ack, socketfd, recv_window);


   
while(buffer_size != 0){

    

    start = 0;
    end = window_size - 1;
    int i = 0;
    buffer_index = 0;
    


    while(buffer_index != -1 ) {
        fill_window(window, buffer, seq_range, &current_seq, shift_index, &buffer_index, window_size);
        
        while(shift_index > 0){
            cout << ": " << end << endl;
            i = (window_size) - shift_index;
            cout << "sequence Number: " << ntohl(window[i].seq_num) << " Data: " << window[i].data << endl;
            //Set time sent variable to time now
            window[i].startTime = std::chrono::steady_clock::now();
            sendto(socketfd, (struct packet*)&window[i], sizeof(window[i]), 0, 
                                    (const struct sockaddr *) &client_addr, sizeof(client_addr));
            shift_index--;
            recv_window[i].sent = true;
            cout << "Packet " << current_packet << " has been sent..." << endl;
            current_packet++;

        }


        unique_lock<mutex> lck(mtx);
        if(recv_flag){  //while loop
            
            if (recv_data.first == "ack"){
                    // cout << recv_data.second << " Bro " << start << " Bro "<<  endl;
                    // Use formula for find_index in Reciever, for insertion
                    recv_window[recv_data.second - start].recieved = true;
                    // cout << "ack " << recv_data.second << " recieved..." << endl;

            } else {
                    window[recv_data.second].startTime = std::chrono::steady_clock::now();
                     sendto(socketfd, (struct packet*)&window[recv_data.second], sizeof(window[recv_data.second]), 0, 
                                (const struct sockaddr *) &client_addr, sizeof(client_addr));
                     cout << "A nak for packet " << recv_data.second << " recieved..." << endl;

            }
            recv_flag = false;
            cv.notify_all();
        }
        // cout << recv_flag << endl;
        lck.unlock();


        // if shift_index is 0, skip check and shiftWindow

        shift_index = slidingCheck(recv_window, window_size);
         

        check(&start,&end,shift_index, seq_range);

        shiftWindow(recv_window, window, shift_index, window_size);

        cout << "does this happen" << endl;

        for(int inc = 0; inc < shift_index; inc++){
            int some = (window_size) - shift_index;
            /* Add sent time variable to packet
             * Check if (time now - sent time in packet) > timeout -- resend packet
             * If resent packet set time sent variable to time now
             */
            auto epoch = window[some].startTime;
            auto win_s = std::chrono::time_point_cast<std::chrono::seconds>(epoch);
            auto value = win_s.time_since_epoch();
            long windowVal = value.count();
            cout << "this is windowVal: " << windowVal << endl;
            double timeout = 8;
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_seconds;
            if(windowVal != 0){
              elapsed_seconds = now - window[some].startTime;  
            }
            if(elapsed_seconds.count() > timeout){
                cout << "A timeout for packet " << window[i].seq_num << " recieved..." << endl;
                
                sendto(socketfd, (struct packet*)&window[some], sizeof(window[some]), 0,
                                        (const struct sockaddr *) &client_addr, sizeof(client_addr));
            }
        }

        

    }    
    buffer_size =  fread(buffer, 1, (sizeof(buffer)), fp);

}
    
    
    

    return 0;
}


// put while loop in main, and split this function up into multiple
bool fill_window(packet window[], char buffer[], int seq_range, int* current_seq, int shift_index, int* buffer_index, int size) {
     
    int i = size - shift_index;
     while(i != size){
        window[i].seq_num = htonl(*current_seq);
        window[i].data_size = htonl(1);
        window[i].data = buffer[*buffer_index];
        i++;
        *current_seq = *current_seq + 1;
        *buffer_index = *buffer_index + 1;

        // memset buffer and read next part of file into it, also reset buffer_index
        if(*buffer_index >= 50){
            *buffer_index = -1;
        }


        
        if(*current_seq >= seq_range){
            cout << seq_range << " - " << *current_seq << " - " << shift_index << " - " << *buffer_index << " - " << size << endl;
            *current_seq = 0;
        }
    }
    return true;
}


void listen_to_ack(int socketfd, rec_send recv_window[]){
    ack ack;

    while(1) {

     recvfrom(socketfd, (struct ack*)&ack ,sizeof(ack),0, NULL, NULL);
     cout << "checkmark"<<endl;

    unique_lock<mutex> lck(mtx);
     if(!ack.nak) {
         recv_data.first = "ack";
         recv_data.second = ntohl(ack.seq_num);

     } else {
         recv_data.first = "nak";
         recv_data.second = ntohl(ack.seq_num);
     }

     recv_flag = true;

     cv.wait(lck);

     


    }

}
/*
Prints out packet information for debugging. 
window: array of packets to print;
all: if true, prints all packets, otherwise, prints singular packet
index: only used if "all" is true, index for the specified packet
*/
int print_packet(packet window[], bool all, int index, int size){
    if(all){
        for(int i =0; i < size; i++){
            cout << "Seq Number: " << window[i].seq_num << "\n";
            cout << "Data_Size: " << window[i].data_size << "\n";
            cout << "Data: " << window[i].data << "\n";

        }
    } else {
        cout << "Seq Number: " << window[index].seq_num << "\n";
        cout << "Data_Size: " << window[index].data_size << "\n";
        cout << "Data: " << window[index].data << "\n";
    }
}




int shiftWindow(rec_send recv_window[], packet window[],  int index, int size){
    if(index == 0) return -1;
    
    for(int i =0 ; i < size - index; i++){
        recv_window[i] = recv_window[i+index];
        recv_window[i+index].sent = false;
        recv_window[i+index].recieved = false;
        window[i] = window[i+index];
        window[i+index] = {};
    }
    return 0;

}





int slidingCheck(rec_send recv_window[], int size) {
    bool pass = false;
    int index = 0; 
    for(int i = 0; i < size; i++) {
        pass = recv_window[i].recieved;
        if(!pass) break;
        index++;
    }
    return index;
}


bool check(int* start, int* end, int shift_index, int seq_range){
    int temp1 = *start;
    int temp2 = *end;
    *start = temp1 + shift_index;
    *end = temp2 + shift_index;

    if(*start > (seq_range - 1 )) {
        *start = temp1 + shift_index - seq_range;
    }

     if(*end > (seq_range - 1)){
         *end = temp2 + shift_index -seq_range;
     }


     return true;



    
}




