#include "socket.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <pthread.h>
#include <ctime>
#include <chrono>
using namespace std::chrono;
class thread_info
{
public:
    int account_id;
    string hostname;
};
void sendString(int client_socket, string msg)
{
    std::string content_to_send = std::to_string(msg.length()) + "\n" + msg;
    int bytes_sent = send(client_socket, content_to_send.c_str(), content_to_send.length(), 0);
    if (bytes_sent < 0)
    {
        std::cerr << "Failed to send content." << std::endl;
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    cout << "bytes send: " << bytes_sent << " msg is: " << content_to_send << endl;
    std::cout << "Content sent successfully." << std::endl;
}
void recvString(int client_socket)
{
    // Receive response from the server
    std::string response(1024, '\0');
    int bytes_received = recv(client_socket, &response[0], response.size(), 0);
    if (bytes_received < 0)
    {
        std::cerr << "Failed to receive response." << std::endl;
    }
    else
    {
        response.resize(bytes_received); // Resize the string to match the received data size
        std::cout << "Received response:\n"
                  << response << std::endl;
    }
}
void *keepFlooding(void *accountID_ptr)
{
    thread_info *inf = (thread_info *)accountID_ptr;
    int account_id = inf->account_id;
    string hostname = inf->hostname;
    try
    {
        int client_socket = create_client(hostname.c_str(), "12345"); //"vcm-31143.vm.duke.edu"
        // Client client("127.0.0.1", PORT);
        cout
            << "Successfully connected" << endl;
        stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><create><account id=\"" << account_id << "\" balance=\"" << 100000000 << "\"/><symbol sym=\"BTC\"><account id=\"" << account_id << "\">300</account></symbol></create>";

        /*
        string accountMsg = "<create>"
                            "<account id=\"" +
                            to_string(account_id) + "\" balance=\"1000\"/>"
                                                    "<symbol sym=\"SPY\">"
                                                    "<account id=\"" +
                            to_string(account_id) + "\">30</account>"
                                                    "</symbol>"
                                                    "<symbol sym=\"ab12\">"
                                                    "<account id=\"" +
                            to_string(account_id) + "\">100</account>"
                                                    "</symbol>"
                                                    "</create>";
        cout << "accountMsg" << accountMsg << endl;
        */
        sendString(client_socket, ss.str());
        recvString(client_socket);

        // Close the socket
        close(client_socket);
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    // Load XML content from a file
    /*
    std::ifstream xml_file("sample2.xml");
    if (!xml_file.is_open())
    {
        std::cerr << "Unable to open XML file." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::stringstream xml_buffer;
    xml_buffer << xml_file.rdbuf();
    xml_file.close();

    std::string xml_content = xml_buffer.str();

    // Add the length of the XML content as a base-10 unsigned integer followed by a newline
    std::string content_to_send = std::to_string(xml_content.length()) + "\n" + xml_content;

    // Create client socket
    int client_socket = create_client("vcm-31143.vm.duke.edu", "12345");
    if (client_socket < 0)
    {
        std::cerr << "Failed to create client socket." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Send the content through the socket
    int bytes_sent = send(client_socket, content_to_send.c_str(), content_to_send.length(), 0);
    if (bytes_sent < 0)
    {
        std::cerr << "Failed to send content." << std::endl;
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Content sent successfully." << std::endl;

    // Receive response from the server
    std::string response(1024, '\0');
    int bytes_received = recv(client_socket, &response[0], response.size(), 0);
    if (bytes_received < 0)
    {
        std::cerr << "Failed to receive response." << std::endl;
    }
    else
    {
        response.resize(bytes_received); // Resize the string to match the received data size
        std::cout << "Received response:\n"
                  << response << std::endl;
    }
    // Close the socket
    close(client_socket);
    */
    if (argc != 3)
    {
        cerr << "two inputs: hostname, num_threads" << endl;
        cerr << "test hostname is: vcm-31143.vm.duke.edu " << endl;
        exit(EXIT_FAILURE);
    }
    pthread_t *threads;
    int numThreads = atoi(argv[2]);
    string hostn = argv[1];
    threads = (pthread_t *)malloc(numThreads * sizeof(pthread_t));
    for (int i = 0; i < numThreads; i++)
    {
        thread_info *inf = new thread_info();
        inf->account_id = i;
        inf->hostname = hostn;
        pthread_create(&threads[i], NULL, keepFlooding, inf);
    }
    for (int i = 0; i < numThreads; i++)
    {
        cout << i << " completed" << endl;
        pthread_join(threads[i], NULL);
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    cout << "The whole process took " << time_span.count() << "seconds." << endl;
    return 0;
}
