#include "hw3.h"

// function to handle errors and exit
void error(char *msg)
{
    perror(msg);
    exit(0);
}

#define BUFFER_SIZE 256 // buffer size for messages

int sockfd; // socket file descriptor for the client

// shread function to receive messages from the server
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        bzero(buffer, BUFFER_SIZE);
        int n = read(sockfd, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
            printf("Disconnected from server\n");
            break;
        }
        buffer[n] = '\0';
        size_t len = strlen(buffer);
        while(len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')){ // check for trailing new line char
            buffer[--len] = '\0'; // remove new line char
        }
        printf("%s\n", buffer); // print the received message
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUFFER_SIZE];

    if (argc < 4) {
       fprintf(stderr,"usage %s hostname port name\n", argv[0]);
       exit(0);
    }

    // create socket, get sockfd handle
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // fill in server address in sockaddr_in datastructure
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    // connect to server
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    // send client name to server
    write(sockfd, argv[3], strlen(argv[3]));

    // create thread for receiving messages
    pthread_t thread;
    pthread_create(&thread, NULL, receive_messages, NULL);

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE - 1, stdin);

        // send user message to server
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0) 
             error("ERROR writing to socket");

        if (strncmp(buffer, "!exit", 5) == 0) { // checking for exit command
            printf("Client exiting\n");
            break;
        }
    }
    // close socket
    close(sockfd);
    return 0;
}
