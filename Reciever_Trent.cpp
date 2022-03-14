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
#include <fstream>

using namespace std;

struct packet {
   
   int seq_num;
   int data_size;
   char data;
};

int read_into_buffer(packet window[], char buffer[], int size);


int main(int argc, char *argv[])
{
    int socketfd, port;
    string ip;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    packet test;
     int buffer_size;
    char buffer[25];
    packet window[25];
    int start = 0;
    int end = 4;
    int i = 0;
    ofstream MyFile("stupid.txt");
    
    socklen_t length;
    
    length = sizeof(client_addr);
    socketfd =  socket(AF_INET, SOCK_DGRAM, 0);
     if (socketfd < 0) 
        perror("ERROR opening socket");
        
    memset(&server_addr, 0, sizeof(server_addr)); 
    
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1070);
    
    
    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
              perror("ERROR on binding");
int j = 0;    
while(j < 4){
    while(i != 25){
         recvfrom(socketfd, (struct packet*)&window[i], sizeof(window[i]),0, NULL, NULL);
         cout << "help";

         i++;

    }
    read_into_buffer(window,buffer,25);
    for(int k =0; k < sizeof(buffer); k++) {
        MyFile << buffer[k];
    }
    j++;

}
MyFile.close();
    return 0;
}

int read_into_buffer(packet window[], char buffer[], int size) {
    int i = 0;
     while(i != size){
        buffer[ ntohl(window[i].seq_num)] = window[i].data;
        i++;

    } 
    return 1 ;
}
