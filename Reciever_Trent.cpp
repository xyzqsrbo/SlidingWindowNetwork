
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



struct packet
{
    int data_size;
    int checksum = 0;
    int seq_num;
    int ip[4];
    int port;
	bool send_ack ;
};

struct state
{
    int seq_range;
    int file_size;
    int packet_size;
    int window_size;
    bool GBN;
};

struct ack
{
    int seq_num;
    bool nak;
    bool fin = false;
};

mutex mtx;

condition_variable seq_alert;
condition_variable ack_alert;
char incoming[512000];
int retransmitted = 0;
int all_packets = 0;

bool ack_flag = false;

bool finished_flag = false;

int write_into_buffer(char *buffer[], fstream &MyFile, int packet_size, int array_index);

int slidingCheck(bool recv_window[], int size);

int shiftWindow(char *buffer[], bool recv_window[], packet window[], int index, int size, int buffer_size);

bool check(int *start, int *end, int shift_index, int seq_range);

int listen_for_packets(int socketfd);

int filesize(fstream &file);

int struct_size(state packet);

int struct_size(packet packet);

bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int packet_size);

int serialize(packet *window, int packet_size);

int findIndex(int start, int end, int seq_num, int seq_range);

int write_into_file(fstream &file, char *buffer[], packet window[], int window_size, int file_size, int shift_index);

void printWindow(bool recv_window[], packet window[], int size);

int main(int argc, char *argv[])
{
    int socketfd, port;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    fstream MyFile("/tmp/testR");

    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        perror("ERROR opening socket");

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_addr.s_addr = inet_addr("10.35.195.220");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9070);
    client_addr.sin_addr.s_addr = inet_addr("10.35.195.202");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(9065);

    state setup;
    ack ack;
    int checking = htonl(1);

    if (::bind(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        perror("ERROR on binding");

    cout << "Waiting for initial packet" << endl;
    recvfrom(socketfd, (struct state *)&setup, sizeof(setup), 0, NULL, NULL);

    int seq_range = ntohl(setup.seq_range);
    int file_size = ntohl(setup.file_size);
    int packet_size = ntohl(setup.packet_size);
    bool GBN = ntohl(setup.GBN);
    int window_size = 0;
    if (GBN)
    {
        window_size = 1;
    }
    else
    {
        window_size = ntohl(setup.window_size);
    }

    int shift_index = window_size;
    packet window[window_size];
    bool recv_window[window_size] = {false};
    int buffer_size = packet_size + struct_size(window[0]);

    cout << "initial packet recieved" << endl;
    //cout << " Seq_range: " << seq_range << "file_size: " << file_size << "packet_size: " << packet_size << endl;

    char **buffer;
    buffer = new char *[window_size];
    for (int i = 0; i < window_size; i++)
    {
        buffer[i] = new char[buffer_size];
    }

    sendto(socketfd, &checking, sizeof(checking), 0,
           (const struct sockaddr *)&client_addr, sizeof(client_addr));

    thread first(listen_for_packets, socketfd);

    int start = 0;
    int end = 0;
    if (GBN)
    {
        end = ntohl(setup.window_size) - 1;
    }
    else
    {
        end = window_size - 1;
    }
    int current_seq = 0;
    int array_index = 0;
    unsigned int data_written = 0;
    packet temp;

    while (data_written < file_size)
    {

        update_sliding_window(window, seq_range, &current_seq, shift_index, window_size, packet_size);
        shift_index = 0;

        while (!ack_flag)
        {
        };
        //cout << "packet_size: " << packet_size << " window_size: " << endl;
        serialize(&temp, packet_size);

        //cout << "Temp Sequence Num: " << ntohl(temp.seq_num) << endl;
        unique_lock<mutex> lck(mtx);

		cout << "Packet " << ntohl(temp.seq_num) << " received" << endl;
        if (GBN)
        {
            array_index = findIndex(start, end, ntohl(temp.seq_num), seq_range);
            if (array_index < 0)
            {
				retransmitted++;
                array_index = 0;
                recv_window[0] = false;
                ack.seq_num = temp.seq_num;
                ack.nak = false;
				if (temp.send_ack) {
                	sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
                       	(const struct sockaddr *)&client_addr, sizeof(client_addr));
				}
				cout << "Ack " << ntohl(temp.seq_num) << " sent" << endl;
				all_packets++;

            }
            else if (array_index == 0)
            {
                window[0] = temp;
                recv_window[0] = true;
                ack.seq_num = temp.seq_num;
                ack.nak = false;
				if (temp.send_ack) {
                	sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
                       	(const struct sockaddr *)&client_addr, sizeof(client_addr));
				}
                write_into_buffer(buffer, MyFile, packet_size, array_index);
				cout << "Ack " << ntohl(temp.seq_num) << " sent" << endl;
				all_packets++;
            }
            else
            {
                if (start == 0)
                {
					retransmitted++;
					all_packets++;
                }
                else
                {
                    recv_window[0] = false;
                    ack.seq_num = htonl(start - 1);
                    ack.nak = false;
					if (temp.send_ack) {
                    	sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
                           	(const struct sockaddr *)&client_addr, sizeof(client_addr));
					}
					cout << "Ack " << ntohl(temp.seq_num) << " sent" << endl;
					all_packets++;
                }
            }
        }
        else
        {
            if (array_index = findIndex(start, end, ntohl(temp.seq_num), seq_range), array_index < 0 || array_index >= window_size)
            {
				retransmitted++;
				all_packets++;
            }
            else if (!recv_window[array_index])
            {
                window[array_index] = temp;
                recv_window[array_index] = true;
                write_into_buffer(buffer, MyFile, packet_size, array_index);

                printWindow(recv_window, window, window_size);

                cout << "Ack sequence Num: " << ntohl(temp.seq_num) << endl;
                cout << "Array Index: " << array_index << endl;

                ack.seq_num = temp.seq_num;
                ack.nak = false;
                ack.fin = false;
				if (temp.send_ack) {
                	sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
                       	(const struct sockaddr *)&client_addr, sizeof(client_addr));
				}
				cout << "Ack " << ntohl(temp.seq_num) << " sent" << endl;
				all_packets++;

            }
        }

        // if return value of slidingcheck is 0, skip check, write, and shift
        shift_index = slidingCheck(recv_window, window_size);

        //cout << "shift_index" << shift_index << endl;

        data_written = data_written + write_into_file(MyFile, buffer, window, window_size, file_size, shift_index);

        //cout << "data Written " << data_written << endl;

        if (data_written >= file_size)
        {
            //cout << "god as my witness" << endl;
            finished_flag = 1;
            seq_alert.notify_all();
            break;
        }

        check(&start, &end, shift_index, seq_range);

        shiftWindow(buffer, recv_window, window, shift_index, window_size, buffer_size);
		
		string windowCurrent = "[";
		for (int i = 0; i < window_size - 1; i++) {
			windowCurrent += std::to_string(ntohl(window[i].seq_num));
			windowCurrent += ", ";
		}
		windowCurrent += std::to_string(ntohl(window[window_size-1].seq_num));
		windowCurrent += "]";

		cout << "Current window = " << windowCurrent << endl;

        ack_flag = false;
        seq_alert.notify_all();
    }
    ack.seq_num = htonl(0);
    ack.nak = false;
    ack.fin = true;
    sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
           (const struct sockaddr *)&client_addr, sizeof(client_addr));


    
    //cout << "file_size: " << file_size << endl;
	


	cout << "Last packet seq# received: " << ntohl(temp.seq_num) << endl;
	cout << "Number of original packets received: " << all_packets - retransmitted << endl;
	cout << "Number of retransmitted packets received: " << retransmitted <<  endl;


    MyFile.close();
	first.join();
    return 0;
}
// check if buffer is full, if so, write into file, and memset buffer, and also reset buffer_index
int write_into_buffer(char *buffer[], fstream &MyFile, int packet_size, int array_index)
{
    int j = 0;
    cout << array_index << endl;

    while (j != packet_size)
    {
        buffer[array_index][j] = incoming[j];
        j++;
    }

    return 1;
}

// for this method, check return. if its negative or if its greater than end, its a past value
int findIndex(int start, int end, int seq_num, int seq_range)
{
    cout << "Array Start: " << start << " Array End: " << end << " Seq Num: " << seq_num << endl;
    if (start == end)
    {
        return 0;
    }
    else if (start < end || seq_num > end)
    {
        return (seq_num - start);
    }
    else
    {
        return (seq_range - start + seq_num);
    }
}

int listen_for_packets(int socketfd)
{

    while (1)
    {

        recvfrom(socketfd, incoming, sizeof(incoming), 0, NULL, NULL);

        unique_lock<mutex> lck(mtx);

        ack_flag = true;
        seq_alert.wait(lck);

        if (finished_flag)
        {
            cout << "thread finished" << endl;
            return 1;
        }
    }
}

int slidingCheck(bool recv_window[], int size)
{
    bool pass = false;
    int index = 0;
    for (int i = 0; i < size; i++)
    {
        pass = recv_window[i];
        if (!pass)
            break;
        index++;
    }
    return index;
}

bool check(int *start, int *end, int shift_index, int seq_range)
{

    *start = *start + shift_index;
    *end = *end + shift_index;

    if (*start > (seq_range - 1))
    {
        *start = *start - seq_range;
    }

    if (*end > (seq_range - 1))
    {
        *end = *end - seq_range;
    }
}

int shiftWindow(char *buffer[], bool recv_window[], packet window[], int index, int size, int buffer_size)
{
    if (index == 0)
        return -1;
    for (int i = 0; i < size; i++)
    {
        if (i + index >= size)
        {
            recv_window[i] = false;
            window[i] = {};
            delete[] buffer[i];
            buffer[i] = new char[51200 + 20];
        }
        else
        {
            recv_window[i] = recv_window[i + index];
            window[i] = window[i + index];
            copy(buffer[i + index], buffer[i + index] + 51200, buffer[i]);
        }
    }
    return 0;
}

int struct_size(packet packet)
{
    int size = 0;
    size += sizeof(packet.data_size);
    size += sizeof(packet.checksum);
    size += sizeof(packet.seq_num);
    size += sizeof(packet.ip);
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

int file_size(fstream &file)
{
    int file_size = 0;
    file.seekg(0, ios_base::end);
    file_size = file.tellg();
    file.clear();
    file.seekg(0);
    return file_size;
}

int write_into_file(fstream &file, char *buffer[], packet window[], int window_size, int file_size, int shift_index)
{
    int i = 0;
    while (i != shift_index)
    {
        cout << "data_size: " << ntohl(window[i].data_size) << endl;
        file.write(buffer[i], ntohl(window[i].data_size));

        cout << "data actually written" << file.tellg() << endl;

        if (file.tellg() >= file_size)
        {

            return file_size;
        }
        i++;
    }

    return (i * 51200);
}

int serialize(packet *window, int packet_size)
{

    memcpy(&(window->seq_num), incoming + packet_size, sizeof(window->seq_num));
    memcpy(&(window->data_size), incoming + packet_size + sizeof(window->seq_num), sizeof(window->data_size));
	memcpy(&(window->send_ack), incoming + packet_size + sizeof(window->seq_num) + sizeof(window->data_size), sizeof(window->send_ack));
    return 0;
}
bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int packet_size)
{
    int i = window_size - shift_index;

    while (i != window_size)
    {
        window[i].seq_num = htonl(*current_seq);
        window[i].data_size = htonl(packet_size);

        *current_seq += 1;
        if (*current_seq >= seq_range)
        {
            *current_seq = 0;
        }
        i++;
    }
}
void printWindow(bool recv_window[], packet window[], int size)
{
    cout << " recv_window: ";
    for (int i = 0; i < size; i++)
    {
        cout << recv_window[i] << " ";
    }
    cout << "\n"
         << endl;

    cout << " window_seq: ";

    for (int i = 0; i < size; i++)
    {
        cout << ntohl(window[i].seq_num) << " ";
    }

    cout << "\n"
         << endl;
}
