#include <stdio.h>
#include <string.h> // strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> // close
#include <arpa/inet.h> // close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> // FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE 1
#define FALSE 0
#define PORT 12345

int main(int argc , char *argv[]) {
    int opt = TRUE;
    int master_socket, addrlen, new_socket, client_socket[30],
        max_clients = 30, activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;
    char buffer[1025]; // Data buffer of 1K
    // Set of socket descriptors
    fd_set readfds;
    // A message to be sent to new clients
    char *message = "ECHO Daemon v1.0 \r\n";
    
    // Initialize all client_socket[] to 0, so they're not checked initially
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    // Create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set master socket to allow multiple connections
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Define the socket type (IPv4, local IP, port 8888)
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind the socket to localhost, port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listener on port %d \n", PORT);
    
    // Try to specify a maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connections
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
    
    while (TRUE) {
        // Clear the socket set
        FD_ZERO(&readfds);
        // Add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Add child sockets to set
        for (i = 0; i < max_clients; i++) {
            // Socket descriptor
            sd = client_socket[i];
            // If valid socket descriptor, then add to read list
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            // Update the highest file descriptor number for select()
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Wait for an activity on one of the sockets (timeout is NULL, so wait indefinitely)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        
        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }

        // If something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept");
                exit(EXIT_FAILURE);
            }
            // Inform user of the new socket number
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            // Send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
                perror("Send");
            }
            puts("Welcome message sent successfully");

            // Add new socket to array of sockets
            for (i = 0; i < max_clients; i++) {
                // If position is empty, add new client socket
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Else, it's some IO operation on an existing client socket
        for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readfds)) {
                // Check if the client closed the connection
                if ((valread = read(sd, buffer, 1024)) == 0) {
                    // Get client details and print the disconnected client info
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected, ip %s, port %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    // Close the socket and mark it as 0 for reuse
                    close(sd);
                    client_socket[i] = 0;
                }
                // Echo back the message to the client
                else {
                    buffer[valread] = '\0';
                    send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
    return 0;
}
