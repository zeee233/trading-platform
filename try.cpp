#include "socket.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <pthread.h>
#include <ctime>
#include <chrono>
int main(int argc, char *argv[])
{
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
    return 0;
}