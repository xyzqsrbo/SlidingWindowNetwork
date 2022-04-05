sender: 
	clang++ -std=c++11 -Wall Sender_Trent.cpp -o sender
receiver: 
	clang++ -std=c++11 -Wall Reciever_Trent.cpp -o receiver
clean: 
	rm sender receiver