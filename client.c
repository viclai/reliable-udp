#include <stdio.h>      // fprintf
#include <stdlib.h>     // atoi
#include <unistd.h>     // getopt
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY
#include <sys/socket.h> // socket, bind, sockaddr

#include <sys/types.h>
#include <netdb.h>      // define structures like hostent
#include <strings.h>
#include <string.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}


int main(int argc, char* argv[])
{
  // TODO: Implement ./client localhost postNumber fileName
    
    // error handling
    if (argc != 4) {
        fprintf(stderr, "must provide 4 arguments");
        exit(0);
    }
    
    //obtain args
    char *hostname = argv[1];
    int portno = atoi(argv[2]);
    char *filename =  argv[3];
    
    //create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0)
        error("ERROR opening socket");
    
     struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    char* fileRequestMsg = (char*)malloc(30*sizeof(char));
    sprintf(fileRequestMsg, "File: %s", filename);
    printf("message says %s", fileRequestMsg);
    if (sendto(sockfd, fileRequestMsg, sizeof(fileRequestMsg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR sending request");
    
    printf("Sent request for file w/o local error %s\n", filename);
        
    
    
        close(sockfd); //close socket
        
        return 0;
    

}
