sender: 
	clang++ -std=c++11 -Wall -pthread Sender_Trent.cpp -o sender
receiver: 
	clang++ -std=c++11 -Wall -pthread Reciever_Trent.cpp -o receiver
clean: 
	rm sender receiver