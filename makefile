all:
	g++ -pthread -std=c++11 -Wall Reciever_Trent.cpp -o r
	g++ -pthread -std=c++11 -Wall Sender_Trent.cpp -o s

clean:
	rm r s
