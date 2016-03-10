#include <noise.h>
#include <channel.h>

#include <stdio.h>      // printf, fprintf, fclose, perror, fopen, sprintf
#include <stdlib.h>     // atoi, atof, exit, malloc, free
#include <unistd.h>     // close
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY
#include <netdb.h>      // gethostbyname
#include <sys/socket.h> // socket, bind, sockaddr, AF_INET, SOCK_DGRAM, recvfrom, sendto
#include <strings.h>    // bcopy
#include <string.h>     // memset, memcpy, strcat, strcpy
#include <errno.h>      // errno

#include <vector>       // vector
#include <map>          // map
using namespace std;

#define UNKNOWN_FILE_LENGTH -1

struct ContentDescriptor {
   char* content;
   int   contentSize;
};

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int nextSeqNum(int curSeqNum) {
     return (curSeqNum + MAX_PACKET_SIZE) % (MAX_SEQUENCE + 1);
}

int seqInWindowRange(int start, int windowSize, int seq) {
    //return 1 if sequence falls within the window range
    //return 0 otherwise
    //accounts for wrapping around
    int end = (start + windowSize) % (MAX_SEQUENCE + 1);
    //printf(" Window from %d to %d\n", start, end);

    if (start < end) {
        return (seq >= start && seq <= end);
    }
    else if (start > end) {
        return (seq >= start || seq  <= end);
    }
    else {
        return seq == start;
    }
}

void adjustWindowAndBuffer(map<int, ContentDescriptor>& m, int& windowStart) {
    //slide window
    //remove entries from old window from map
    map<int, ContentDescriptor>::iterator it = m.find(windowStart);
    if (it != m.end())
    {
        if (it->second.content != NULL)
            delete it->second.content;
    }
    m.erase(windowStart);
    windowStart = nextSeqNum(windowStart);
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
        if (cur == '\0' || i == MAX_PACKET_SIZE) {
            end = i;
            break;
        }
        i++;
    }
    int contentLength = end - start; // account for excluding null byte
    memcpy(contents, newBuffer + start, contentLength);
    //contents[contentLength] = '\0';
    //printf("contents\n%s\n", contents);
    
    free(windowSizeString);
    free(fileSizeString);
    free(seqNumString);
    return contentLength;
}

int main(int argc, char* argv[])
{
    // error handling
    if (argc != 6) {
        fprintf(stderr, "./client [hostname] [portno] [filename] [prob loss] [prob corrupt");
        exit(0);
    }
    
    //obtain args
    char *hostname = argv[1];
    int portno = atoi(argv[2]);
    char *filename =  argv[3];
    double pLoss = atof(argv[4]);
    double pCorrupt = atof(argv[5]);

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
    //printf("message says %s", fileRequestMsg);
    if (sendto(sockfd, fileRequestMsg, strlen(fileRequestMsg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR sending request");
    
    free(fileRequestMsg);
    printf("Sent request for file w/o local error %s\n", filename);
    
    char buffer[MAX_PACKET_SIZE];
    socklen_t slen = sizeof(struct sockaddr_in);
    int count = 0;
    int totalLength = 0;
    int fileSize = UNKNOWN_FILE_LENGTH;
    FILE *file = fopen(strcat(filename, "_1"), "wb"); //to do, fix filename
    
    //create map from sequence num --> contents / content size
    //unseen sequence numbers will not be in the map
    map<int, ContentDescriptor> receivedSequence;

    //keep track of next expected seq num
    //int expSeqNum = 0;
    
    //keep track of window 
    //int seqInWindow = 0;

    //window start 
    int windowStart = 0;
    //window size unknown;
    int windowSize = -1; 


    while (fileSize == UNKNOWN_FILE_LENGTH || totalLength < fileSize) {
        memset(buffer, 0, sizeof(buffer));
        bool isLost = simulatePacketLossCorruption(pLoss);
        bool isCorrupt = simulatePacketLossCorruption(pCorrupt);
        //printf("lost: %d corrupt: %d\n", (int)isLost, (int)isCorrupt);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &serv_addr, &slen);
        
        if (n == -1 || isLost) {
            //don't send anything
            printf("Packet lost |");
            //printf("an error: %s\n", strerror(errno));
        }
        else if (isCorrupt) {
            //don't send anything
            printf("Packet is corrupt");
        }
        else if (n == 0) {
            printf("empty\n");
        }
        else {
            //printf("Received %d bytes\n", n);
            //printf("buffer is %s\n", buffer);
            
            //copy buffer contents
            char* newBuffer = (char*)malloc(MAX_PACKET_SIZE);
            memset(newBuffer, 0, sizeof(buffer));
            memcpy(newBuffer, buffer, MAX_PACKET_SIZE);
            //printf("newbuffer is %s\n", newBuffer);
            //free(buffer);

            
            int seqNum;
            char* contents;
            contents = (char*)malloc(MAX_PACKET_SIZE);
            memset(contents, 0, MAX_PACKET_SIZE);
            int contentLength = parseChunk(newBuffer, seqNum, fileSize, contents, windowSize);
            printf("*** SEQ %d RECEIVED ***\n", seqNum);
            //printf("* WindowStart: %d", windowStart);
            if (seqInWindowRange(windowStart, windowSize, seqNum) && receivedSequence.find(seqNum) == receivedSequence.end()) {
                //sequence # has not been seen before and is in the window
                //printf("%d is unseen sequence num in window| ", seqNum);
                //store contents in temp buffer
                ContentDescriptor c;
                c.content = (char*)malloc(MAX_PACKET_SIZE);
                strcpy(c.content, contents);
                c.contentSize = contentLength;
                receivedSequence[seqNum] = c;
                if (seqNum != windowStart) {
                    //printf("%d not next expected number (%d) | ", seqNum, windowStart);
                    //printf("storing seq %d in buffer, content length is %d | ", seqNum, contentLength);
                }
                else {
                    //write to file and shift window
                    //printf("%d is next expected number | ", windowStart);
                    totalLength+= contentLength;
                    fwrite(contents, sizeof(char), contentLength, file);
                    adjustWindowAndBuffer(receivedSequence, windowStart);

                    //write any contiguous previously stored packets
                    while (totalLength < fileSize && receivedSequence.find(windowStart) != receivedSequence.end()) {
                            //printf(" seq %d previously stored in buffer \n", windowStart);
                            ContentDescriptor c = receivedSequence[windowStart];
                            totalLength += c.contentSize;
                            fwrite(c.content, sizeof(char), c.contentSize, file);
                            adjustWindowAndBuffer(receivedSequence, windowStart);

                    }
                }
                
                //printf("seqNum is %d\n", seqNum);
                //printf("file size is %d\n", fileSize);
            }
            else {
                //ignore
                printf("Duplicate sequence\n");
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
                printf("Sent %s \n", ack);
                count++;
            }
            
            free(ack);
            free(contents);
            free(newBuffer);
        }
        printf("%d/%d\n", totalLength, fileSize);
    }

    //print contents to file

    fclose(file);
    close(sockfd); //close socket

    return 0;
}
