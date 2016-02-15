#include <stdio.h>      // fprintf
#include <string.h>     // memset
#include <stdlib.h>     // atoi, exit
#include <fcntl.h>      // fcntl
#include <unistd.h>     // usleep
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY
#include <sys/types.h>  
#include <netdb.h>      // hostent, gethostbyaddr
#include <sys/select.h> // select
#include <sys/socket.h> // socket, bind, sockaddr, AF_INET, SOCK_DGRAM
#include <sys/time.h>   // FD_SET, FD_ISSET, FD_ZERO, gettimeofday, timeval

#include <string>
#include <iostream>
using namespace std;

const int BUFFER_SIZE = 1024;
const int TIMEOUT = 4;

int getClientMsg(int sockfd, string& msg, int flags,
                 struct sockaddr* clientAddr,
                 socklen_t* addrLen);

int sendMsg(int sockfd, const void* buffer, size_t length, int flags,
            struct sockaddr* destAddr, socklen_t destLen);

int main(int argc, char* argv[])
{
  // Usage: ./server portNumber
  // Server is the sender
  // Send the packet with the file content to the client

  int sockfd, port, n, nRead, activity;
  string clientMsg, clientName;
  struct sockaddr_in serverAddr, clientAddr;
  socklen_t clientAddrLen;
  struct hostent* clientHost;
  fd_set readFds;

  if (argc < 2)
  {
    fprintf(stderr, "ERROR: no port provided\n");
    exit(1);
  }

  port = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    perror("ERROR opening socket");
    exit(1);
  }

  memset((char*)&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port); // Converts host byte order to network
                                     // byte order

  if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
  {
    perror("ERROR binding");
    exit(1);
  }

  while (true)
  {
    FD_ZERO(&readFds);

    FD_SET(sockfd, &readFds);

    // TODO: Set timeout
    activity = select(sockfd + 1, &readFds, NULL, NULL, NULL);
    if (activity < 0)
      fprintf(stdout, "Select error\n");
    if (FD_ISSET(sockfd, &readFds))
    {
      fprintf(stdout, "Receiving a packet...\n");

      if ((nRead = getClientMsg(sockfd, clientMsg, 0,
                                (struct sockaddr*)&clientAddr,
                                &clientAddrLen)) == 0)
      {
        // Disconnected
        close(sockfd);
        break;
      }
      else
      {
        // TODO: Process client message

        if ((clientHost = gethostbyaddr(&clientAddr.sin_addr.s_addr,
                                        sizeof(clientAddr.sin_addr.s_addr),
                                        AF_INET)))
        {
          clientName = clientHost->h_name;
        }
        else
          clientName = "Unknown Client";

        cout << "Message from " << clientName << ": " <<
                clientMsg << "\n";
        const char* msg = "I got your message!\n";
        n = sendMsg(sockfd, msg, 20, 0, (struct sockaddr*)&clientAddr,
                    clientAddrLen);
        if (n < 0)
        {
          perror("ERROR sending to socket");
          exit(1);
        }
      }
    }
  }

  return 0;
}

int getClientMsg(int sockfd, string& msg, int flags,
                 struct sockaddr* clientAddr,
                 socklen_t* addrLen)
{
  int nRead, totalRead = 0;
  char buffer[BUFFER_SIZE];
  struct timeval now, start;
  double diff;
  int opts;

  opts = fcntl(sockfd, F_SETFL, O_NONBLOCK); // Make the socket non-blocking

  gettimeofday(&start, NULL);

  while (1)
  {
    gettimeofday(&now, NULL);

    diff = 
      (now.tv_sec - start.tv_sec) + (1E-6 * (now.tv_usec - start.tv_usec));

    if (totalRead > 0 && diff > TIMEOUT)
      break;
    else if (diff > 2 * TIMEOUT)
      break;

    memset(buffer, 0, BUFFER_SIZE);
    if ((nRead =
           recvfrom(sockfd, buffer, BUFFER_SIZE, 0, clientAddr, addrLen)) < 0)
    {
      usleep(100000); // No data so try again after sleeping
    }
    else
    {
      totalRead += nRead;

      string temp(buffer);
      msg += temp;

      gettimeofday(&start, NULL); // Reset starting time
    }
  }
  fcntl(sockfd, F_SETFL, opts & (~O_NONBLOCK)); // Reset socket to blocking
  return totalRead;
}

int sendMsg(int sockfd, const void* buffer, size_t length, int flags,
            struct sockaddr* destAddr, socklen_t destLen)
{
  ssize_t n;
  const char* p = (const char*)buffer;

  while (length > 0)
  {
    n = sendto(sockfd, p, length, flags, destAddr, destLen);
    if (n <= 0)
      break;
    p += n;
    fprintf(stdout, "%d bytes sent to client\n", (int)n);
    length -= n;
  }
  return (n > 0) ? 0 : -1;
}
