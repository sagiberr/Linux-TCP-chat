#include "hw3.h"
typedef struct {
   int sockfd; // file descriptor for the socket
   struct sockaddr_in addr; // server adress
   char name[50]; // client name
}Client;

Client *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex=PTHREAD_MUTEX_INITIALIZER;

// normal message
// function to broadcast a messsage to all clients including the sender
void broadcast_message(const char *message)
{
   int i;
   pthread_mutex_lock(&clients_mutex);
   for(i=0;i<MAX_CLIENTS;i++)
   {
      // if client exist at index i, send the message to the client i
      if(clients[i] != NULL)
      {
         write(clients[i]->sockfd,message, strlen(message)); 
      }
   }

   pthread_mutex_unlock(&clients_mutex);
}

// disconnection
// function to handle client disconnection
void handle_client_disconnection(Client *client)
{
   int i;
   close(client->sockfd); // close the socket 
   pthread_mutex_lock(&clients_mutex);
   for(i=0;i<MAX_CLIENTS;i++)
   {
      if(clients[i]->name==client->name) // if the client i is the client that we wish to disconnect
      {
         clients[i]=NULL;
         break;
      }

   }
   pthread_mutex_unlock(&clients_mutex);

   printf("%s disconnected\n",client->name);
   free(client);

}

// whisper message
void handle_whisper_message(char *buffer, char *source_client){ // if we get error with whispers, come back here
   // Handle whisper messages
   char *recipient_name = strtok(buffer + 1, " ");
   char *message = strtok(NULL, "");
   if (recipient_name && message) {
      pthread_mutex_lock(&clients_mutex);
      for (int i = 0; i < MAX_CLIENTS; ++i) {
         if (clients[i] && strcmp(clients[i]->name, recipient_name) == 0) {
            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, BUFFER_SIZE, "%s: %s\n", source_client, message);
            write(clients[i]->sockfd, formatted_message, strlen(formatted_message));
            break;
         }
      }
         pthread_mutex_unlock(&clients_mutex);
   }
}


// thread function to handle communication with the client
void *client_handler(void *arg)
{
   char buffer[BUFFER_SIZE];
   Client *client = (Client *)arg;

   snprintf(buffer, BUFFER_SIZE, "%s connected from %s\n",
             client->name, inet_ntoa(client->addr.sin_addr));
   printf("%s", buffer);

   while(1){
      bzero(buffer, BUFFER_SIZE); // clear the buffer
      int n = read(client->sockfd, buffer, BUFFER_SIZE - 1);
      if (n<=0){
         handle_client_disconnection(client); // client has disconnected
         break;
      }
      buffer[n] = '\0';
      if (strncmp(buffer, "@", 1) == 0){ // check for whisper
         handle_whisper_message(buffer, client->name);
      }
      else{
         //assume that the messsage+name <256
         char formatted_message[2*BUFFER_SIZE]; // add client name prefix and send message to all clients
         snprintf(formatted_message, 2*BUFFER_SIZE, "%s: %s\n", client->name, buffer);
         broadcast_message(formatted_message);
      }
   }
   return NULL;
}

// function to handle error messages
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
   int sockfd, newsockfd, portno;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;

   if (argc < 2) {
      fprintf(stderr,"ERROR, no port provided\n");
      exit(1);
   }

   // create socket
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) 
      error("ERROR opening socket");

   // fill in port number to listen on. IP address can be anything (INADDR_ANY)
   bzero((char *) &serv_addr, sizeof(serv_addr));
   portno = atoi(argv[1]);
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);

   // bind socket to this port number on this machine
   if (bind(sockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0) 
            error("ERROR on binding");
   
   // listen for incoming connection requests
   listen(sockfd, 5);
   printf("Server listening on port %d\n", portno);
   clilen = sizeof(cli_addr);

   while (1) {
      // accept a new connection
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      if (newsockfd < 0) 
            error("ERROR on accept");

      // create a new client object
      Client *client = malloc(sizeof(Client));
      client->sockfd = newsockfd;
      client->addr = cli_addr;

      // read the client's name
      bzero(client->name, sizeof(client->name));
      read(newsockfd, client->name, sizeof(client->name) - 1);
      client->name[strcspn(client->name, "\n")] = 0; // Remove newline

      // add the client to the clients array
      pthread_mutex_lock(&clients_mutex);
      int added = 0;
      for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i]) {
               clients[i] = client;
               pthread_t thread;
               pthread_create(&thread, NULL, client_handler, client);
               pthread_detach(thread);
               added = 1;
               break;
            }
      }
      pthread_mutex_unlock(&clients_mutex);
      if (!added){ // if client not added, close the new socket
         close(newsockfd);
         free(client);
         printf("Connection refused: Maximum clients reached.\n");
      }
   }
   // close socket
   close(sockfd);
   return 0; 
}

