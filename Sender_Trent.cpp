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

using namespace std;


struct packet {
   
   int seq_num;
   int data_size;
   char data;
};


int create_window(packet window[], char buffer[], int current_seq, int size);





int main(int argc, char *argv[])
{
    // Initialize socket variables
    
    int socketfd, port;
    int current_seq = 0;
    string ip;
    struct sockaddr_in server_addr, client_addr;
    packet test;
    int buffer_size;
    char buffer[25];
    packet window[25];
    int start = 0;
    int end = 4;


    FILE * fp = fopen("storyofmylife.txt", "r");
    socketfd =  socket(AF_INET, SOCK_DGRAM, 0);
     if (socketfd < 0) 
        perror("ERROR opening socket");
    
    memset(&server_addr, 0, sizeof(server_addr)); 
    
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(1070);

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1065);
    
    
    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) 
              perror("ERROR on binding");
    buffer_size =  fread(buffer, 1, sizeof(buffer), fp);

while(buffer_size > 0){
    create_window(window, buffer, current_seq, 25);
    cout << buffer[4];
    cout << window[4].data;
    cout << window[4].data_size;
    cout << window[4].seq_num;


    for(int i = 0; i < 25; i++) {
        sendto(socketfd, (struct packet*)&window[i], sizeof(window[i]), 0, 
                                (const struct sockaddr *) &client_addr, sizeof(client_addr));
        end++;
    }

   
    buffer_size =  fread(buffer, 1, sizeof(buffer), fp);

}
    
    
    

    return 0;
}

int create_window(packet window[], char buffer[], int current_seq, int size) {
    int i = 0;
     while(i != size){

        window[i].seq_num = htonl(current_seq);
        window[i].data_size = htonl(1);
        window[i].data = buffer[i];
        current_seq++;
        i++;

    }
    return i;
}