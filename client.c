//PROJECT 2

#include <stdio.h>      // fprintf
#include <stdlib.h>     // atoi
#include <unistd.h>     // getopt
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY
#include <sys/socket.h> // socket, bind, sockaddr

#include <sys/types.h>
#include <netdb.h>      // define structures like hostent
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <noise.h>
#include <vector>   
#include <map>

#define UNKNOWN_FILE_LENGTH -1
#define MAX_PACKET_SIZE 1024
#define MAX_SEQ_NUM 30720

using namespace std;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int nextSeqNum(int curSeqNum) {
     return (curSeqNum + MAX_PACKET_SIZE) % (MAX_SEQ_NUM + 1);
}

int parseChunk(char* newBuffer, int& seqNum, int&fileSize, char* contents, int &windowSize) {
    //printf("new buffer is\n %s\n", newBuffer);
    //stores sequence number, file size, and file contents
    //returns size of file
    
    //TO DO:
    //strtok was causing problems, so brute force for now
    //maybe optimize if time later
    
    int i = 0;
    int start, end;
    
    //obtain seq num
    while (1) {
        char cur = *(newBuffer + i);
        if (cur == ':') {
            start = i+2;
        }
        else if (cur == '\n') {
            end = i-1;
            break;
        }
        i++;
    }
    int seqLength = end - start + 1;
    char* seqNumString = (char*)malloc(seqLength+1);
    seqNumString[seqLength] = '\0';
    memcpy(seqNumString, newBuffer + start, seqLength);
    seqNum = atoi(seqNumString);
    //printf("seq num is %d\n", seqNum);


    //obtain file size
    while (1) {
        char cur = *(newBuffer + i);
        if (cur == ':') {
            start = i+2;
        }
        else if (cur == 'B') {
            end = i-1;
            break;
        }
        i++;
    }
    

    int length = end - start + 1;
    char* fileSizeString = (char*)malloc(length+1);
    fileSizeString[length] = '\0';
    memcpy(fileSizeString, newBuffer + start, length);
    fileSize = atoi(fileSizeString);
    //printf("f size is %d\n", fileSize);
    

    //obtain window size 
    i+= 1;
    while (1) {
        char cur = *(newBuffer + i);
        if (cur == ':') {
            start = i+2;
        }
         if (cur == 'B') {
            end = i-1;
            break;
        }
        i++;
    }

    int length_ws = end - start + 1;
    char* windowSizeString = (char*)malloc(length_ws+1);
    windowSizeString[length_ws] = '\0';
    memcpy(windowSizeString, newBuffer + start, length_ws);
    windowSize = atoi(windowSizeString);

    //obtain contents
    start = i+3;
    while (1) {
        char cur = *(newBuffer + i);
        if (cur == '\0') {
            end = i;
            break;
        }
        i++;
    }
    
    int contentLength = end - start; // account for excluding null byte
    memcpy(contents, newBuffer + start, contentLength);
    //contents[contentLength] = '\0';
    //printf("contents\n%s\n", contents);
    
    return contentLength;
}

int main(int argc, char* argv[])
{
  // TODO: Implement ./client localhost postNumber fileName
    
    // error handling
    if (argc != 6) {
        fprintf(stderr, "./client [hostname] [portno] [filename] [prob loss] [prob corrupt");
        exit(0);
    }
    
    //obtain args
    char *hostname = argv[1];
    int portno = atoi(argv[2]);
    char *filename =  argv[3];
    int pLoss = atof(argv[4]);
    int pCorrupt = atof(argv[5]);

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
    if (sendto(sockfd, fileRequestMsg, strlen(fileRequestMsg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR sending request");
    
    printf("Sent request for file w/o local error %s\n", filename);
    
    char buffer[MAX_PACKET_SIZE];
    socklen_t slen = sizeof(struct sockaddr_in);
    int count = 0;
    int totalLength = 0;
    int fileSize = UNKNOWN_FILE_LENGTH;
    FILE *file = fopen(strcat(filename, "_1"), "wb"); //to do, fix filename
    
    //create map of which have sequences have been received
    map<int, int> receivedSequence;

    //keep track of size of vectors
    int maxIndex = MAX_SEQ_NUM/MAX_PACKET_SIZE + 1;

    //create vector of each sequence contents
    vector<char*> contentsinSequence(maxIndex, nullptr);

    //create vector of each sequence content size
    vector<int> contentSizeinSequence(maxIndex, 0);

    //keep track of next expected seq num
    int expSeqNum = 0;
    
    //keep track of window 
    int seqInWindow = 0;
    while (fileSize == UNKNOWN_FILE_LENGTH || totalLength < fileSize) {
        memset(buffer, 0, sizeof(buffer));
        int isLost = simulatePacketLossCorruption(pLoss);
        int isCorrupt = simulatePacketLossCorruption(pCorrupt);
        //printf("lost: %d corrupt: %d\n", isLost, isCorrupt);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &slen);
        
        if (n == -1 || isLost) {
            //don't send anything
            printf("packet lost\n");
            //printf("an error: %s\n", strerror(errno));
        }
        else if (isCorrupt) {
            //don't send anything
            printf("packet is corrupt\n");
        }
        else if (n == 0) {
            printf("empty\n");
        }
        else {
            printf("Received %d bytes\n", n);
            //printf("buffer is %s\n", buffer);
            
            //copy buffer contents
            char* newBuffer = (char*)malloc(MAX_PACKET_SIZE);
            memcpy(newBuffer, buffer, MAX_PACKET_SIZE);
            
            
            int seqNum;
            int windowSize;
            char* contents;
            contents = (char*)malloc(MAX_PACKET_SIZE);
            memset(contents, 0, MAX_PACKET_SIZE);
            //printf("raw message is %s\n", newBuffer);
            int contentLength = parseChunk(newBuffer, seqNum, fileSize, contents, windowSize);
            printf("*** SEQ %d RECEIVED ***\n", seqNum);
            
            if (receivedSequence.find(seqNum) == receivedSequence.end()) {
                //sequence # has not been seen before
                receivedSequence[seqNum] = 1;
                // if not next number, save to buffer
                if (seqNum != expSeqNum) {
                    contentsinSequence[seqNum] = contents;
                    contentSizeinSequence[seqNum] = contentLength;
                    printf("storing seq %d in buffer, content length is %d | ", seqNum, contentLength);
                }
                else {
                    //write to file
                    printf("writing seq %d to file | ", expSeqNum);
                    totalLength+= contentLength;
                    fwrite(contents, sizeof(char), contentLength, file);
                    expSeqNum = nextSeqNum(expSeqNum);
                    seqInWindow+= 1024;
                    //write any contiguous previously stored packets
                    while (/*seqInWindow < windowSize && */totalLength < fileSize) {
                        if (receivedSequence.find(expSeqNum) != receivedSequence.end()) {
                            printf(" seq %d available in buffer ", expSeqNum);
                            int l = contentSizeinSequence[expSeqNum];
                            char* contInBuffer = contentsinSequence[expSeqNum];

                            printf("content  is %s | ", contInBuffer);

                            totalLength += l;
                            fwrite(contInBuffer, sizeof(char), l, file);
                            expSeqNum = nextSeqNum(expSeqNum);
                            seqInWindow+=1024;
                        }
                        else {
                            break;
                        }
                    }
                    /*
                    //TO DO: fix later for larger windows/ files

                    //reset vectors and map when seq start to repeat
                    if (seqInWindow == windowSize) {
                        printf("resetting window\n");
                        contentsinSequence.clear();
                        contentSizeinSequence.clear();
                        receivedSequence.clear();
                        seqInWindow = 0;
                    }
                    */
                }
                
                
                //printf("seqNum is %d\n", seqNum);
                //printf("file size is %d\n", fileSize);
            }
            else {
                //ignore
                printf("duplicate sq\n");
            }
            
            //printf("Contents are\n%s", contents);
            //printf("Window size is %d\n", windowSize);
            
            //send ack
            char* ack = (char*)malloc(32);
            sprintf(ack, "ACK: %d", seqNum);
            if (sendto(sockfd, ack, strlen(ack), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                printf("error\n");
            }
            else {
                //printf("ack success\n");
                printf("sent %s | ", ack);
                count++;
            }

        }
        printf("written %d out of %d\n", totalLength, fileSize);
    }

    //print contents to file

    fclose(file);
    close(sockfd); //close socket
        
        return 0;
    

}
