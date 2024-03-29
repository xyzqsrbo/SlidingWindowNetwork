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
#include <vector>
#include <fstream>
#include <chrono>
using namespace std;

mutex mtx;

condition_variable cv;

/* packets for sending message
ip - sender ip address
seq_num - sequence number of data
data_size - size of data
data - data to be sent
*/

// global variables for user input
string protocol;
int packet_size;
int timeoutInterval;
int timeout;
int windowSize;
int seq_range;
string errors;
int retransmitted = 0;
bool GBN = false;
bool SW = false;
bool SR = false;
vector<bool> ack_errors;
vector<bool> packet_errors;


struct packet
{
    int data_size;
    int checksum = 0;
    int seq_num;
    int ip[4] = {23, 43, 56, 45};
    int port = 4567;
    std::chrono::time_point<std::chrono::steady_clock> time_sent;
	bool send_ack =true;
};

struct state
{
    int seq_range;
    int file_size;
    int packet_size;
    int window_size;
    bool GBN;
};

/* struct for recieving acknowledgements from reciever
seq_num sequence number of acknowledged package
nak - bool for problems - if true, package was damaged
*/
struct ack
{
    int seq_num;
    bool nak;
    bool fin;
};

/* our sl
 */

struct rec_send
{
    bool sent = false;

    bool recieved = false;
};

pair<string, int> recv_data;

bool recv_flag = false;

bool donzo = false;

bool fill_window(packet window[], char buffer[], int seq_range, int *current_seq, int shift_index, int *buffer_index, int size);

bool check(int *start, int *end, int shift_index, int seq_range);

int slidingCheck(rec_send recv_window[], int size);

int shiftWindow(char *buffer[], rec_send recv_window[], packet window[], int index, int size, int packet_size, int structsize);

int print_packet(packet window[], bool all, int index);

int listen_to_ack(int socketfd, rec_send recv_window[]);

int load_data(packet window[], FILE *fp, int packet_size, int window_index);

int findIndex(int start, int end, int seq_num, int seq_range);

int read_into_buffer(ifstream &file, char *buffer[], int packet_size, int window_size, int begin);

int filesize(ifstream &file);

int struct_size(state packet);

int struct_size(packet packet);

bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int data_read, int packet_size);

int serialize(char buffer[], packet window, int buffer_size, int packet_size);

void bufferPrint(char *buffer[]);

int checksum(char buffer[], int buffer_size);

void userInput();

void printWindow(rec_send recv_window[], packet window[], int size);

int main(int argc, char *argv[])
{
	userInput();

	if(SW){
		windowSize = 1;
	}

    // Declaration of variables for our socket
    int socketfd, port;
    struct sockaddr_in server_addr, client_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    /*
    Client is for the recieving end/destination
    Server is for the sending end/source
    Need Port # and IP Address for both.
    */
    client_addr.sin_addr.s_addr = inet_addr("10.35.195.220");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(9070);

    server_addr.sin_addr.s_addr = inet_addr("10.35.195.202");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9065);

    const int window_size = windowSize;
    //const int seq_range = 32;
    //int packet_size = 51200;
    int shift_index = window_size;
	packet *window;
    //packet window[window_size];
    window = new packet[window_size];
	int buffer_size = packet_size + struct_size(window[0]);
    char **buffer;
    buffer = new char *[window_size];
    cout << "struct" << struct_size(window[0]);
    for (int i = 0; i < window_size; i++)
    {
        buffer[i] = new char[buffer_size];
    }
	
	rec_send *recv_window;
    //rec_send recv_window[window_size];
	recv_window = new rec_send[window_size];
    int checking = 0;

    int current_packet = 0;
    int current_seq = 0;

    ifstream file;
    file.open("/tmp/1g");
    int file_size = filesize(file);

    state setup = {htonl(seq_range), htonl(file_size), htonl(packet_size), htonl(window_size), htonl(GBN)};

    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        perror("ERROR opening socket");

    if (::bind(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        perror("ERROR on binding");


	std::chrono::time_point<std::chrono::steady_clock> start_now;
	std::chrono::time_point<std::chrono::steady_clock> start_end;


	if (timeoutInterval == 1) {
		start_now = std::chrono::steady_clock::now();
	}

    if ((sendto(socketfd, (struct state *)&setup, sizeof(setup), 0,
                (const struct sockaddr *)&client_addr, sizeof(client_addr))) < 0)
    {
        perror("ERROR on initial Sending");
        int fail = 0;
        do
        {
            fail = sendto(socketfd, (struct state *)&setup, sizeof(setup), 0,
                          (const struct sockaddr *)&client_addr, sizeof(client_addr));
        } while (fail < 0);
    }

    recvfrom(socketfd, &checking, sizeof(checking), 0, NULL, NULL);

	if (timeoutInterval == 1) {
		start_end = std::chrono::steady_clock::now();
		timeout = double(std::chrono::duration_cast<std::chrono::milliseconds>(start_end - start_now).count()) +3.0;
	}


    if (checking)
    {
        cout << "Reciever has gotten initial setup" << endl;
    }

    thread first(listen_to_ack, socketfd, recv_window);

    int data_read = 0;

    int start = 0;
    int end = window_size - 1;
    int i = 0;
    int ind = 0;

	//timer for the program
	auto timeStart = std::chrono::steady_clock::now();

	cout << "begin: " << window_size - shift_index << endl;
    while (!donzo)
    {
        data_read = read_into_buffer(file, buffer, packet_size, window_size, window_size - shift_index);


        update_sliding_window(window, seq_range, &current_seq, shift_index, window_size, data_read, packet_size);

        while (shift_index > 0)
        {

            //cout << "End Window: " << end << endl;
            i = (window_size)-shift_index;
            //cout << "seq_num sent: " << htonl(window[i].seq_num) << " packet_size: " << sizeof(buffer[i]) << endl;
            // Set time_sent variable to time now
            window[i].time_sent = std::chrono::steady_clock::now();

			 for (int z = 0; z < ack_errors.size(); z++)
            {
                if (ntohl(window[i].seq_num) == ack_errors[z])
                {
                    window[i].send_ack = false;
                }
            }
            serialize(buffer[i], window[i], data_read, packet_size);
            //dont send a packets in a sequence number if requested
            for (int z = 0; z < packet_errors.size(); z++)
            {
                if (window[i].seq_num == packet_errors[z])
                {
                    shift_index--;
                    recv_window[i].sent = true;
                    continue;
                }
            }

            serialize(buffer[i], window[i], data_read, packet_size);
            //cout << "Checksum is : " << checksum(buffer[i], buffer_size) << endl;
            /*if (window[i].seq_num == 100663296)
            {
                cout << "Losing packet " << window[i].seq_num << endl;
                shift_index--;
                recv_window[i].sent = true;
                continue;
            }*/
            //cout << "buffer sent " << buffer[i][0] << buffer[i][1] << buffer[i][2] << endl;
			cout << "Packet " << ntohl(window[i].seq_num) << " Sent" << endl;
            sendto(socketfd, buffer[i], packet_size + struct_size(window[0]), 0,
                   (const struct sockaddr *)&client_addr, sizeof(client_addr));
            shift_index--;
            recv_window[i].sent = true;
            //cout << "Packet " << current_packet << " has been sent..." << endl;
            current_packet++;
        }

        while (!recv_flag)
        {
            for (int ii = 0; ii < window_size; ii++)
            {

                /* Add sent time variable to packet
                 * Check if (time now - sent time in packet) > timeout -- resend packet
                 * If resent packet set time sent variable to time now
                 */
                auto epoch = window[ii].time_sent;
                auto win_s = std::chrono::time_point_cast<std::chrono::seconds>(epoch);
                auto value = win_s.time_since_epoch();
                long windowVal = value.count();

                auto now = std::chrono::steady_clock::now();
                double elapsed_seconds;

                if (windowVal != 0)
                {
                    elapsed_seconds = double(std::chrono::duration_cast<std::chrono::milliseconds> ( now - window[ii].time_sent).count());
                }
                if (elapsed_seconds > timeout && recv_window[ii].sent && !recv_window[ii].recieved)
                {
					window[ii].send_ack = true;
                    cout << "A timeout for packet " << ntohl(window[ii].seq_num) << " recieved..." << endl;

                    window[ii].time_sent = std::chrono::steady_clock::now();
                    serialize(buffer[ii], window[ii], data_read, packet_size);
                    sendto(socketfd, buffer[ii], packet_size + struct_size(window[0]), 0,
                           (const struct sockaddr *)&client_addr, sizeof(client_addr));

					cout << "Packet " << ntohl(window[ii].seq_num) << " Re-Transmitted." << endl; 
                	retransmitted++;
				}
            }
        }

        if (donzo)
        {
            break;
        }

        printWindow(recv_window, window, window_size);
        unique_lock<mutex> lck(mtx);

        if (ind = findIndex(start, end, recv_data.second, seq_range), ind < 0 || ind >= window_size)
        {
        }
        else
        {

            //cout << "Array_index: " << ind << endl;
            if (recv_data.first == "ack")
            {
                // cout << recv_data.second << " Bro " << start << " Bro "<<  endl;
                // Use formula for find_index in Reciever, for insertion
                if (GBN)
                {
                    for (int k = 0; k < ind; k++)
                    {
                        recv_window[k].recieved = true;
                    }
                }
                recv_window[ind].recieved = true;

                cout << "ack " << ntohl(recv_data.second) << " recieved..." << endl;
            }
            else
            {
            }
        }
        recv_flag = false;
        cv.notify_all();

        // cout << recv_flag << endl;

        // if shift_index is 0, skip check and shiftWindow

        shift_index = slidingCheck(recv_window, window_size);

        //cout << "shiftIndex: " << shift_index << endl;
        check(&start, &end, shift_index, seq_range);
        shiftWindow(buffer, recv_window, window, shift_index, window_size, packet_size, struct_size(window[0]));

		string windowCurrent = "[";
		for (int i = 0; i < window_size-1; i++) {
			
			windowCurrent += std::to_string(ntohl(window[i].seq_num));
			windowCurrent += ", ";
		}
		windowCurrent += std::to_string(ntohl(window[window_size-1].seq_num));
		windowCurrent += "]";

		cout << "Current window = " << windowCurrent << endl;
    }

	auto timeEnd = std::chrono::steady_clock::now();

	double elapsed_time = double(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count())/1000;

	cout << "Session sucessfully terminated" << endl;
	cout << endl;

	cout << "Number of original packets sent: " << current_packet << endl;
	cout << "Number of retransmitted packts: " << retransmitted << endl;
	cout << "Total elapsed time: " <<  elapsed_time << endl;
	double effectiveThroughput = ((file_size/1000)/1000)/elapsed_time;
	cout << "Total throughput (Mbps): " << effectiveThroughput + ((retransmitted*packet_size)/1000/1000/elapsed_time) << endl;
	cout << "Effective throughput (Mbps): " << effectiveThroughput<< endl; 
	
	
	file.close();

	first.join();

    return 0;
}

// put while loop in main, and split this function up into multiple
bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int data_read, int packet_size)
{
    int i = window_size - shift_index;

    while (i != window_size)
    {
        //cout << *current_seq << endl;
        window[i].seq_num = htonl(*current_seq);
        if (i == (window_size - 1))
        {
            window[i].data_size = htonl(data_read);
        }
        else
        {
            window[i].data_size = htonl(packet_size);
        }

        *current_seq += 1;
        if (*current_seq >= seq_range)
        {
            *current_seq = 0;
        }
        i++;
    }

    return true;
}

int listen_to_ack(int socketfd, rec_send recv_window[])
{
    ack ack;

    while (1)
    {

        recvfrom(socketfd, (struct ack *)&ack, sizeof(ack), 0, NULL, NULL);
        //cout << "checkmark" << endl;

        unique_lock<mutex> lck(mtx);
        if (!ack.nak)
        {
            recv_data.first = "ack";
            recv_data.second = ntohl(ack.seq_num);
            cout << "recieved ack num: " << recv_data.second << endl;
        }
        else
        {
            recv_data.first = "nak";
            recv_data.second = ntohl(ack.seq_num);
        }

        if (ack.fin)
        {
            donzo = true;
            recv_flag = true;
            return 1;
        }
        else
        {
            donzo = false;
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
int print_packet(packet window[], bool all, int index, int size)
{
    if (all)
    {
        for (int i = 0; i < size; i++)
        {
            cout << "Seq Number: " << window[i].seq_num << "\n";
            cout << "Data_Size: " << window[i].data_size << "\n";
        }
    }
    else
    {
        cout << "Seq Number: " << window[index].seq_num << "\n";
        cout << "Data_Size: " << window[index].data_size << "\n";
    }
}

int shiftWindow(char *buffer[], rec_send recv_window[], packet window[], int index, int size, int packet_size, int structsize)
{
    if (index == 0)
        return -1;
    for (int i = 0; i < size; i++)
    {
        if (i + index >= size)
        {
            recv_window[i].sent = false;
            recv_window[i].recieved = false;
            window[i] = {};
            delete[] buffer[i];
            buffer[i] = new char[packet_size + structsize];
        }
        else
        {
            recv_window[i] = recv_window[i + index];
            window[i] = window[i + index];
            copy(buffer[i + index], buffer[i + index] + packet_size, buffer[i]);
        }
    }
    return 0;
}

int findIndex(int start, int end, int seq_num, int seq_range)
{

    //cout << "Array Start: " << start << " Array End: " << end << " Seq Num: " << seq_num << endl;
    if(start == end){
		return 0;
	}
	if (start < end || seq_num > end)
    {
        return seq_num - start;
    }
    else
    {
        return seq_range - start + seq_num;
    }
}

int slidingCheck(rec_send recv_window[], int size)
{
    bool pass = false;
    int index = 0;
    for (int i = 0; i < size; i++)
    {
        pass = recv_window[i].recieved;
        if (!pass)
            break;
        index++;
    }
    return index;
}

bool check(int *start, int *end, int shift_index, int seq_range)
{
    int temp1 = *start;
    int temp2 = *end;
    *start = temp1 + shift_index;
    *end = temp2 + shift_index;

    if (*start > (seq_range - 1))
    {
        *start = temp1 + shift_index - seq_range;
    }

    if (*end > (seq_range - 1))
    {
        *end = temp2 + shift_index - seq_range;
    }

    return true;
}

int struct_size(packet packet)
{
    int size = 0;
    size += sizeof(packet.ip);
    size += sizeof(packet.data_size);
    size += sizeof(packet.checksum);
    size += sizeof(packet.seq_num);
    size += sizeof(packet.time_sent);
    size += sizeof(packet.port);
    return size;
}

int struct_size(state packet)
{
    int size = 0;
    size += sizeof(packet.seq_range);
    size += sizeof(packet.file_size);
    size += sizeof(packet.packet_size);
    return size;
}

int filesize(ifstream &file)
{
    int file_size = 0;
    file.seekg(0, ios_base::end);
    file_size = file.tellg();
    file.clear();
    file.seekg(0);
    return file_size;
}

int read_into_buffer(ifstream &file, char *buffer[], int packet_size, int window_size, int begin)
{
    //cout << "beginning" << endl;

    int i = begin;
    while (i != window_size)
    {
        file.read(buffer[i], packet_size);

        if (packet_size > file.gcount())
        {
            return file.gcount();
        }
        i++;
    }

    //cout << "ending" << endl;

    return file.gcount();
}

int serialize(char buffer[], packet window, int buffer_size, int packet_size)
{

    memcpy(buffer + packet_size, &window.seq_num, sizeof(window.seq_num));
    memcpy(buffer + packet_size + sizeof(window.seq_num), &window.data_size, sizeof(window.data_size));
    memcpy(buffer + packet_size + sizeof(window.seq_num) + sizeof(window.data_size), window.ip, sizeof(window.ip));
    memcpy(buffer + packet_size + sizeof(window.seq_num) + sizeof(window.data_size) + sizeof(window.ip), &window.port, sizeof(window.port));
	memcpy(buffer + packet_size + sizeof(window.seq_num) + sizeof(window.data_size) + sizeof(window.ip) + sizeof(window.port), &window.send_ack, sizeof(window.send_ack));
    return 0;
}

void userInput()
{
    char check;
    cout << "Would you like to use default values? (y/n) ";
    cin >> check;

    if (check == 'n')
    {
        cout << "" << endl;
	   int protocol = 0;
	   while (!GBN && !SR && !SW) {

       cout << "Type of protocol (Go-back N (0), Selective Repeat (1), or Stop and Wait (2)): ";
	   cin >> protocol;
	   cout<< endl;
	   	   switch (int(protocol))
	   {
		case 0:
			GBN = true;
			break;
		case 1:
			SR = true;
			break;
		case 2:
			SW = true;
			break;
		default:
			cout << "Retry!" << endl;
			

	   }
	   }
	   
	   cout << "Size of packet: ";
	   cin >> packet_size;
	   cout << endl;
	   bool Continue = false;
	   
	   while (!Continue) {
	   	cout << "Timeout interval (User Selected (0) or Ping Calculated (1)): ";
	   	cin >> timeoutInterval;
	   	cout << endl;

		switch (timeoutInterval) {
			case 0:
				cout << "What time (ms) do you want to timeout at? ";
				cin >> timeout;
				cout << endl;
				Continue = true;
				break;
			case 1:
				cout << "Thanks for choosing ping calculated!" << endl;
				Continue = true;
				break;
			default:
				cout << "retry" << endl;
				timeoutInterval = 1;
		}   
    }
		if(!SW){
			cout << "Size of window: ";
			cin >> windowSize;
			cout << endl;
		}


		cout << "Size of sequence numbers: ";
		cin >> seq_range;
		cout << endl;

		char want;

		cout << "Do you want to simulate ack loss? (y/n) ";
		cin >> want;
		if (want == 'y') {

		// losing acks
        int error;
        cout << "Ack sequence number you want to lose: ";
        cin >> error;
        cout << endl;
        ack_errors.push_back(error);
        while (1)
        {
            cout << "Do you want to lose more acks? Yes(1)/No(2) ";
            int keepgoing;
            cin >> keepgoing;
            cout << endl;
            if (keepgoing == 2)
            {
                break;
            }
            cout << "Ack sequence number you want to lose: ";
            cin >> error;
            cout << endl;
            ack_errors.push_back(error);
        }
		}

	
		cout << "Do you want to simulate packet loss? (y/n) ";
		cin >> want;
		if (want == 'y') {
		int error;
        // losing packets
        cout << "Packet sequence number you want to lose: ";
        cin >> error;
        cout << endl;
        packet_errors.push_back(error);
        while (1)
        {
            cout << "Do you want to lose more packets? Yes(1)/No(2) ";
            int keepgoing;
            cin >> keepgoing;
            cout << endl;
            if (keepgoing == 2)
            {
                break;
            }
            cout << "Packet sequence number you want to lose: ";
            cin >> error;
            cout << endl;
            packet_errors.push_back(error);
        }
		}


	}
    else
    {
        // input default values for the above variables to run the program
		SW = true;
        packet_size = 51200;
        timeout = 4;
        seq_range = 32;
		windowSize = 4;
    }
}

void printWindow(rec_send recv_window[], packet window[], int size)
{
    //cout << "recv_window: ";
    for (int i = 0; i < size; i++)
    {
        //cout << recv_window[i].recieved << " ";
    }
    //cout << "\n"<< endl;

    //cout << "window_seq: ";

    for (int i = 0; i < size; i++)
    {
        //cout << ntohl(window[i].seq_num) << " ";
    }

    //cout << "\n" << endl;
}

void bufferPrint(char *buffer[])
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 1024; j++)
        {
            //cout << buffer[i][j];
        }
        //cout << "\n" << endl;
    }
}

int checksum(char buffer[], int buffer_size)
{
    unsigned long sum = 0;
    int i = 0;

    while (i != buffer_size)
    {

        sum += (unsigned short)buffer[i];
        if (sum & 0xFFFF0000)
        {

            sum &= 0xFFFF;
            sum++;
        }

        i++;
    }

    return ~(sum & 0xFFFF);
}
