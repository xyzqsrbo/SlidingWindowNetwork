
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
    int ip = 0;
    int data_size;
    int checksum = 0;
    int seq_num;
    int time_sent = 0;
};

struct state
{
    int seq_range;
    int file_size;
    int packet_size;
};

struct ack
{
    int seq_num;
    bool nak;
};

mutex mtx;

condition_variable seq_alert;
condition_variable ack_alert;
char incoming[64000];

bool ack_flag = false;

int write_into_buffer(char *buffer[], fstream &MyFile, int packet_size, int array_index);

int slidingCheck(bool recv_window[], int size);

int shiftWindow(char *buffer[], bool recv_window[], packet window[], int index, int size);

bool check(int *start, int *end, int shift_index, int seq_range);

int listen_for_packets(packet window[], bool recv_window[], int socketfd);

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
    const int window_size = 4;
    int seq_range = 0;
    int packet_size = 0;

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
    int file_size = 0;
    int data_written = 0;
    int array_index = 0;
    packet temp;

    fstream MyFile("stupid.txt");

    socklen_t length;

    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        perror("ERROR opening socket");

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1070);

    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(1065);
    state setup;
    ack ack;
    int checking = htonl(1);

    if (::bind(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        perror("ERROR on binding");

    cout << "Waiting for initial packet" << endl;
    recvfrom(socketfd, (struct state *)&setup, sizeof(setup), 0, NULL, NULL);
    seq_range = ntohl(setup.seq_range);
    file_size = ntohl(setup.file_size);
    packet_size = ntohl(setup.packet_size);

    cout << "initial packet recieved" << endl;
    cout << "Seq_range: " << seq_range << " file_size: " << file_size << " packet_size: " << packet_size << endl;

    char **buffer;
    buffer = new char *[window_size];
    for (int i = 0; i < window_size; i++)
    {
        buffer[i] = new char[packet_size + struct_size(window[0])];
    }

    sendto(socketfd, &checking, sizeof(checking), 0,
           (const struct sockaddr *)&client_addr, sizeof(client_addr));

    thread first(listen_for_packets, window, recv_window, socketfd);

    start = 0;
    end = window_size - 1;
    while (data_written < file_size)
    {
        int j = 0;

        while (j != window_size)
        {
            update_sliding_window(window, seq_range, &current_seq, shift_index, window_size, packet_size);
            shift_index = 0;

            while (!ack_flag)
            {
            };
            cout << "packet_size: " << packet_size << " window_size: " << endl;
            serialize(&temp, packet_size);

            cout << "Temp Sequence Num: " << temp.seq_num << endl;
            unique_lock<mutex> lck(mtx);
            // If array index is outside of window it was a past value so leave it alone
            if (array_index = findIndex(start, end, ntohl(temp.seq_num), seq_range), array_index < 0 || array_index >= window_size)
            {
            }
            // If array index is inside window but the ack has not been done yet send the ack and write to file
            else if (!recv_window[array_index])
            {
                window[array_index] = temp;
                recv_window[array_index] = true;
                write_into_buffer(buffer, MyFile, packet_size, array_index);

                printWindow(recv_window, window, window_size);

                cout << "Ack sequence Num: " << ntohl(ack.seq_num) << endl;
                cout << "Array Index: " << array_index << endl;

                ack.seq_num = temp.seq_num;
                ack.nak = false;

                sendto(socketfd, (struct ack *)&ack, sizeof(ack), 0,
                       (const struct sockaddr *)&client_addr, sizeof(client_addr));
            }

            // if return value of slidingcheck is 0, skip check, write, and shift
            shift_index = slidingCheck(recv_window, window_size);

            cout << "shift_index" << shift_index << endl;

            data_written = data_written + write_into_file(MyFile, buffer, window, window_size, file_size, shift_index);

            cout << "data Written " << data_written << endl;

            if (data_written >= file_size)
            {
                break;
            }

            check(&start, &end, shift_index, seq_range);

            shiftWindow(buffer, recv_window, window, shift_index, window_size);

            ack_flag = false;
            seq_alert.notify_all();

            j++;
        }
    }
    MyFile.close();
    close(socketfd);
    cout << "You made it fucker" << endl;
    return 0;
}
// check if buffer is full, if so, write into file, and memset buffer, and also reset buffer_index
int write_into_buffer(char *buffer[], fstream &MyFile, int packet_size, int array_index)
{
    int j = 0;
    cout << "Array Index: " << array_index << endl;

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
    if (start < end || seq_num > end)
    {
        return (seq_num - start);
    }
    else
    {
        return (seq_range - start + seq_num);
    }
}

int listen_for_packets(packet window[], bool recv_window[], int socketfd)
{

    while (1)
    {

        recvfrom(socketfd, incoming, sizeof(incoming), 0, NULL, NULL);

        unique_lock<mutex> lck(mtx);

        ack_flag = true;
        seq_alert.wait(lck);
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

int shiftWindow(char *buffer[], bool recv_window[], packet window[], int index, int size)
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
            buffer[i] = new char[1024 + 20];
        }
        else
        {
            recv_window[i] = recv_window[i + index];
            window[i] = window[i + index];
            copy(buffer[i + index], buffer[i + index] + 1024, buffer[i]);
        }
    }
    return 0;
}

int struct_size(packet packet)
{
    int size = 0;
    size += sizeof(packet.ip);
    size += sizeof(packet.data_size);
    size += sizeof(packet.checksum);
    size += sizeof(packet.seq_num);
    size += sizeof(packet.time_sent);
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

        if (file.tellg() >= file_size)
        {

            return file_size;
        }
        i++;
    }

    return (i * ntohl(window[i - 1].data_size));
}

int serialize(packet *window, int packet_size)
{

    memcpy(&(window->seq_num), incoming + packet_size, sizeof(window->seq_num));
    memcpy(&(window->data_size), incoming + packet_size + sizeof(window->seq_num), sizeof(window->data_size));
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
