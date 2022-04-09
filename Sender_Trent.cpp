
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
int packetSize;
string timeoutInterval;
int windowSize;
int rangeStart;
int rangeEnd;
string errors;
double timeout;
std::chrono::time_point<std::chrono::steady_clock> first_sent;

struct packet
{

    int ip = 0;
    int data_size;
    int checksum = 0;
    int seq_num;
    std::chrono::time_point<std::chrono::steady_clock> time_sent;
};

struct state
{
    int seq_range;
    int file_size;
    int packet_size;
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

bool fill_window(packet window[], char buffer[], int seq_range, int *current_seq, int shift_index, int *buffer_index, int size);

bool check(int *start, int *end, int shift_index, int seq_range);

int slidingCheck(rec_send recv_window[], int size);

int shiftWindow(char *buffer[], rec_send recv_window[], packet window[], int index, int size);

int print_packet(packet window[], bool all, int index);

void listen_to_ack(int socketfd, rec_send recv_window[]);

int load_data(packet window[], FILE *fp, int packet_size, int window_index);

int findIndex(int start, int end, int seq_num, int seq_range);

int read_into_buffer(ifstream &file, char *buffer[], int packet_size, int window_size, int begin);

int filesize(ifstream &file);

int struct_size(state packet);

int struct_size(packet packet);

bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int data_read, int packet_size);

int serialize(char buffer[], packet window, int buffer_size, int packet_size);

void bufferPrint(char *buffer[]);

void userInput();

void printWindow(rec_send recv_window[], packet window[], int size);

bool donzo = false;

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

    const int window_size = 4;
    const int seq_range = 8;
    int packet_size = 1024;
    int shift_index = 4;
    packet window[window_size];
    char **buffer;
    buffer = new char *[window_size];
    for (int i = 0; i < window_size; i++)
    {
        buffer[i] = new char[packet_size + struct_size(window[0])];
    }

    rec_send recv_window[window_size];
    int checking = 0;

    int current_packet = 0;
    int buffer_index = 0;

    int start;
    int end;
    int current_seq = 0;

    ifstream file;
    file.open("testfile");
    int file_size = filesize(file);

    state setup = {htonl(seq_range), (int)htonl(file_size), (int)htonl(packet_size)};

    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        perror("ERROR opening socket");

    if (::bind(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        perror("ERROR on binding");
    // for automatic timeout interval setting
    auto start_now = std::chrono::steady_clock::now();
    if ((sendto(socketfd, (struct state *)&setup, sizeof(setup), 0,
                (const struct sockaddr *)&client_addr, sizeof(client_addr))) < 0)
    {
        perror("ERROR on initial Sending");
        int fail = 0;
        do
        {
            // for automatic timeout interval setting
            auto start_now = std::chrono::steady_clock::now();
            fail = sendto(socketfd, (struct state *)&setup, sizeof(setup), 0,
                          (const struct sockaddr *)&client_addr, sizeof(client_addr));
        } while (fail < 0);
    }

    recvfrom(socketfd, &checking, sizeof(checking), 0, NULL, NULL);
    auto start_end = std::chrono::steady_clock::now();
    timeout = double(std::chrono::duration_cast<std::chrono::milliseconds>(start_end - start_now).count()) + 3.0;

    if (checking)
    {
        cout << "Reciever has gotten initial setup" << endl;
    }

    thread first(listen_to_ack, socketfd, recv_window);

    int data_read = 0;
    start = 0;
    end = window_size - 1;
    int i = 0;
    buffer_index = 0;
    int ind = 0;

    // Try to find the last flag available and while that hasen't been found.
    while (!donzo)
    {
        data_read = read_into_buffer(file, buffer, packet_size, window_size, window_size - shift_index);

        update_sliding_window(window, seq_range, &current_seq, shift_index, window_size, data_read, packet_size);

        // This checks for timeouts while the file is not done being acknowledged
        for (int ii = 0; ii < window_size; ii++)
        {
            /* Add sent time variable to packet
             * Check if (time now - sent time in packet) > timeout -- resend packet
             * If resent packet set time sent variable to time now
             */
            // This is used to find if the packet has been sent yet -- i.e., time_sent!=0
            auto epoch = window[ii].time_sent;
            auto win_s = std::chrono::time_point_cast<std::chrono::milliseconds>(epoch);
            auto value = win_s.time_since_epoch();
            long windowVal = value.count();

            // Timeout in milliseconds if user specified
            // timeout = 100;
            // Time no in nanoseconds
            auto now = std::chrono::steady_clock::now();
            double elapsed_seconds = 0.0;
            if (windowVal != 0)
            {
                // convert time sent(ns) and now(ns) to milliseconds and subtract them
                elapsed_seconds = double(std::chrono::duration_cast<std::chrono::milliseconds>(now - window[ii].time_sent).count());
            }
            // if elapsed_seconds equal or above the timeout in milliseconds reset sent time(ns) and send the packet
            if (elapsed_seconds >= timeout /*&& recv_window[ii].sent && !recv_window[ii].recieved*/)
            {
                cout << "A timeout for packet " << ntohl(window[ii].seq_num) << " recieved..." << endl;

                window[ii].time_sent = std::chrono::steady_clock::now();
                serialize(buffer[ii], window[ii], data_read, packet_size);
                sendto(socketfd, buffer[ii], packet_size + struct_size(window[0]), 0,
                       (const struct sockaddr *)&client_addr, sizeof(client_addr));
            }
        }

        while (shift_index > 0 && data_read != -1)
        {
            i = (window_size)-shift_index;
            // Set time_sent variable to time now
            window[i].time_sent = std::chrono::steady_clock::now();
            serialize(buffer[i], window[i], data_read, packet_size);
            if(ntohl(window[i].seq_num) == 1){
                cout << "Losing packet " << ntohl(window[i].seq_num) << endl;
                shift_index--;
                recv_window[i].sent = true;
                continue;
            }
            sendto(socketfd, buffer[i], packet_size + struct_size(window[0]), 0,
                   (const struct sockaddr *)&client_addr, sizeof(client_addr));
            shift_index--;
            recv_window[i].sent = true;
        }
        while(!recv_flag){
        };

        printWindow(recv_window, window, window_size);
        unique_lock<mutex> lck(mtx);

        if (ind = findIndex(start, end, recv_data.second, seq_range), ind < 0 || ind >= window_size)
        {
        }
        else
        {

            if (recv_data.first == "ack")
            {
                // Use formula for find_index in Reciever, for insertion
                recv_window[ind].recieved = true;
            }
            else
            {
            }
        }
        recv_flag = false;
        cv.notify_all();

        // if shift_index is 0, skip check and shiftWindow

        shift_index = slidingCheck(recv_window, window_size);

        check(&start, &end, shift_index, seq_range);
        shiftWindow(buffer, recv_window, window, shift_index, window_size);
    }

    cout << "Done sending files and receiver all acks!!!" << endl;

    close(socketfd);
    return 0;
}

// put while loop in main, and split this function up into multiple
bool update_sliding_window(packet window[], int seq_range, int *current_seq, int shift_index, int window_size, int data_read, int packet_size)
{
    int i = window_size - shift_index;

    while (i != window_size)
    {
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

void listen_to_ack(int socketfd, rec_send recv_window[])
{
    ack ack;

    while (1)
    {

        recvfrom(socketfd, (struct ack *)&ack, sizeof(ack), 0, NULL, NULL);
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
            cout << "Seq Number: " << window[i].seq_num << endl;
            cout << "Data_Size: " << window[i].data_size << endl;
        }
    }
    else
    {
        cout << "Seq Number: " << window[index].seq_num << endl;
        cout << "Data_Size: " << window[index].data_size << endl;
    }
}

int shiftWindow(char *buffer[], rec_send recv_window[], packet window[], int index, int size)
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

int findIndex(int start, int end, int seq_num, int seq_range)
{
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
    int i = begin;
    while (i != window_size)
    {
        file.read(buffer[i], 1024);

        if (!file.gcount())
        {
            return -1;
        }
        i++;
    }
    return file.gcount();
}

int serialize(char buffer[], packet window, int buffer_size, int packet_size)
{

    memcpy(buffer + packet_size, &window.seq_num, sizeof(window.seq_num));
    memcpy(buffer + packet_size + sizeof(window.seq_num), &window.data_size, sizeof(window.data_size));
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

        cout << "Type of protocol (GBN, SR, or SAW): ";
        cin >> protocol;

        cout << "Size of packet: ";
        cin >> packetSize;

        cout << "Timeout interval (US or PC): ";
        cin >> timeoutInterval;

        cout << "Size of sliding window: ";
        cin >> windowSize;

        cout << "Range of sequence numbers: ";
        cin >> rangeStart;
        cin >> rangeEnd;

        cout << "Situational errrors: ";
        cin >> errors;
    }
    else
    {
        // input default values for the above variables to run the program
    }
}

void printWindow(rec_send recv_window[], packet window[], int size)
{
    cout << "recv_window: ";
    for (int i = 0; i < size; i++)
    {
        cout << recv_window[i].recieved << " ";
    }
    cout << "\n"
         << endl;

    cout << "window_seq: ";

    for (int i = 0; i < size; i++)
    {
        cout << ntohl(window[i].seq_num) << " ";
    }

    cout << "\n"
         << endl;
}

void bufferPrint(char *buffer[])
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 1024; j++)
        {
            cout << buffer[i][j];
        }
        cout << "\n"
             << endl;
    }
}
