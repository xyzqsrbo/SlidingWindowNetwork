#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <fstream>

using namespace std;

int main(){
	//create a socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1){
		return 1;
	}

	//create a hint structure for the server we're connecting with
	int port = 54000;
	string ipAddress = "127.0.0.1";

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);
	
	//connect to the serveron the socket
	int connectRes = connect(sock, (sockaddr*)&hint, sizeof(hint));
	if(connectRes == -1){
		return 1;
	}

	//while loop:
	char buf[4096];
	string userInput;
	//do{
		//enter lines of text
		/*cout << "> ";
		getline(cin, userInput);
		*/
		ifstream myfile;
		myfile.open("myfile.txt");
		string mystring;
		int line = 0;
		if(myfile.is_open()){
		       	while(myfile){
				getline(myfile, userInput);
				//send to server
				int sendRes = send(sock, userInput.c_str(), userInput.size() + 1, 0);
				if(sendRes == -1){
					cout << "Could not send to server!" << endl;
					continue;
				}
				memset(buf, 0, 4096);
				int bytesReceived = recv(sock, buf, 4096, 0);
				if(line ==0){
					cout << "SERVER> " << string(buf, bytesReceived) << "\r\n";
					line++;
				}else if(string(buf, bytesReceived).find("ack") != string::npos){
					cout << "Server> " << string(buf, bytesReceived) << "\r\n";
				}/*
				//wait for response
				memset(buf, 0, 4096);
				int bytesReceived = recv(sock, buf, 4096, 0);

				//display response
				cout << "SERVER> " << string(buf, bytesReceived) << "\r\n";
				*/
			}
			myfile.close();
		}
		else{ cout << "Could not open file!" << endl;}
	//}while(true);

	//close the socket
	close(sock);

	return 0;
}
