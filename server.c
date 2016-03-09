#include "server.h"
#include "channel.h"
#include "noise.h"

#include <stdlib.h>     // atoi, exit, malloc, free, atol, atof
#include <signal.h>     // signalaction
#include <netinet/in.h> // sockaddr_in, htons, htonl, INADDR_ANY
#include <unistd.h>     // close
#include <errno.h>      // errno
#include <time.h>       // tm, localtime, time
#include <sys/select.h> // select
#include <sys/socket.h> // socket, bind, sockaddr, AF_INET, SOCK_DGRAM,
                        // recvfrom, sendto
#include <sys/time.h>   // FD_SET, FD_ISSET, FD_ZERO, gettimeofday, timeval,
                        // setitimer
#include <stdio.h>      // fprintf, fopen, ftell, fclose, fseek, rewind,
                        // SEEK_END, perror, sprintf

#include <string>       // string, to_string, c_str
#include <cstring>      // strcpy, strchr, strstr, memset, memcpy
#include <vector>       // vector
#include <list>         // list
#include <utility>      // pair, make_pair
#include <sstream>      // ostringstream
using namespace std;

int main(int argc, char* argv[])
{
  int sockfd, port, n, activity, windowSize, sequence, ret;
  string clientMsg, clientName;
  struct sockaddr_in serverAddr, clientAddr;
  socklen_t addrLen;
  struct sigaction timeAction, intAction;
  fd_set readFds;
  const char* serverMsg;
  pair<MessageType, string> typeValue;
  MessageType msgType;
  double corruptProb, lossProb;
  bool isLost;

  intAction.sa_handler = freeClient;
  sigaction(SIGINT, &intAction, NULL);

  if (argc < 5)
  {
    fprintf(stderr,
      "Usage: %s PORT WINDOW-SIZE PROB-LOSS PROB-CORRUPT\n", argv[0]);
    exit(1);
  }

  port = atoi(argv[1]);

  windowSize = atoi(argv[2]);
  if (windowSize > MAX_SEQUENCE / 2)
  {
    fprintf(stderr, "Window size is too large\n");
    exit(1);
  }
  if (windowSize < MAX_PACKET_SIZE)
  {
    fprintf(stderr, "Window size must be at least %dB\n", MAX_PACKET_SIZE);
    exit(1);
  }

  lossProb = atof(argv[3]);
  if (lossProb < 0.0 || lossProb > 1.0)
  {
    fprintf(stderr,
      "Probability of packet loss must be between 0 and 1 inclusive\n");
    exit(1);
  }

  corruptProb = atof(argv[4]);
  if (corruptProb < 0.0 || corruptProb > 1.0)
  {
    fprintf(stderr,
      "Probability of corruption must be between 0 and 1 inclusive\n");
    exit(1);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    perror("ERROR opening socket");
    exit(1);
  }

  memset((char*)&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
  {
    perror("ERROR binding");
    exit(1);
  }

  timeAction.sa_handler = catchAlarm;
  timeAction.sa_flags = SA_RESTART;
  sigaction(SIGALRM, &timeAction, NULL);

  addrLen = sizeof(clientAddr);

  clientReq = new SRInfo;
  clientReq->sequenceSpace.cwnd = windowSize;

  while (true)
  {
    FD_ZERO(&readFds);
    FD_SET(sockfd, &readFds);
    isLost = false;

    if (timeout_flag)
      alertTimeout();
    activity = select(sockfd + 1, &readFds, NULL, NULL, NULL);
    if (activity < 0)
    {
      if (errno != EINTR)
        fprintf(stderr, "* Select error (#%d)\n", errno);
    }
    else if (activity == 0)
    {
      // This part should never be reached
      continue;
    }
    else
    {
      if (FD_ISSET(sockfd, &readFds))
      {
        if (timeout_flag)
          alertTimeout();
        else
          fprintf(stdout, "*\n");

        if (clientReq->filemeta.name != "" &&
            simulatePacketLossCorruption(lossProb))
        {
          fprintf(stdout, "* Lost packet!\n");
          isLost = true;
        }
        else
          fprintf(stdout, "* Receiving a packet...\n");

        if ((ret = recvMsg(sockfd, clientMsg, 0, (struct sockaddr*)&clientAddr,
                         &addrLen)) == 0)
        {
          delete clientReq;
          clientReq = new SRInfo;
          clientReq->sequenceSpace.cwnd = windowSize;
          fprintf(stdout,
            "* Client disconnected: accepting new file request\n");
          continue;
        }
        else if (ret == -1)
        {
          fprintf(stderr, "* Receive error (#%d)\n", errno);
          exit(1);
        }
        else
        {
          if (isLost)
            continue;
          if (timeout_flag)
            alertTimeout();
          fprintf(stdout, "* %s Incoming message: %s\n", get_time().c_str(),
            clientMsg.c_str());
          fprintf(stdout, "* Computing checksum...\n");

          if (clientReq->filemeta.name != "" &&
              simulatePacketLossCorruption(corruptProb))
          {
            fprintf(stdout, "* Packet is corrupted!\n");
            continue;
          }

          fprintf(stdout, "* Packet is intact!\n");

          if (timeout_flag)
            alertTimeout();

          // Process client message
          typeValue = parseMsg(clientMsg);
          msgType = typeValue.first;
          if (msgType == REQUEST)
          {
            clientReq->clientInfo.address = (struct sockaddr*)&clientAddr;
            clientReq->clientInfo.length = addrLen;
            clientReq->clientInfo.sockfd = sockfd;

            if (fileExists(&clientReq->filemeta))
            {
              fprintf(stdout, "* Requested file (%d B) exists\n",
                clientReq->filemeta.length);
            }
            else
            {
              fprintf(stdout, "* Requested file does not exist\n");
            }
            fprintf(stdout, "* Preparing packet(s) for '%s'...\n",
              clientReq->filemeta.name.c_str());
            createSegments();
          }
          else if (msgType == ACK)
          {
            sequence = atoi(typeValue.second.c_str());
            fprintf(stdout, "* %s Processing ACK...\n", get_time().c_str());
            processAck(&clientReq->sequenceSpace, sequence);
            if (timeout_flag)
              alertTimeout();
          }
          else // Message with unknown format
          {
            serverMsg = "Unable to process your message\n";
            n = sendMsg(sockfd, serverMsg, strlen(serverMsg), 0,
                        (struct sockaddr*)&clientAddr, addrLen);
            if (n < 0)
            {
              perror("* ERROR sending to client address");
              exit(1);
            }
            continue;
          }

          // Send packets if window is not full
          sendPackets(&clientReq->sequenceSpace, sockfd,
                      (struct sockaddr*)&clientAddr, addrLen);
          if (timeout_flag)
            alertTimeout();
        }
      }
    }
  }

  return 0;
}

FileData::FileData()
{
  length = 0;
}

Ack::Ack()
{
  isAcked = false;
}

AckSpace::AckSpace()
{
  windowSize = cwnd = base = nextSeq = 0;
}

int recvMsg(int sockfd, string& msg, int flags, struct sockaddr* clientAddr,
            socklen_t* addrLen)
{
  char buffer[MAX_PACKET_SIZE];
  int nRead;

  msg = "";
  memset(buffer, 0, MAX_PACKET_SIZE);

  if ((nRead = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, clientAddr,
       addrLen)) < 0)
    return -1;
  msg += buffer;
  return nRead;
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
    length -= n;
  }
  return (n > 0) ? n : -1;
}

pair<MessageType, string> parseMsg(string message)
{
  int len, startIndex;
  char* value; // Either file name or ACK sequence number
  char* msg;
  char* space;
  pair<MessageType, string> typeValue;

  msg = new char[message.size() + 1];
  strcpy(msg, message.c_str());
  if (strstr(msg, "File") == msg)
    typeValue.first = REQUEST;
  else if (strstr(msg, "ACK") == msg)
    typeValue.first = ACK;
  else
  {
    typeValue.first = UNKNOWN;
    return typeValue;
  }

  space = strchr(msg, ' ');
  startIndex = space - msg + 1;
  len = message.size() - startIndex + 1;

  value = new char[len];
  memcpy(value, &msg[startIndex], len - 1);
  value[len - 1] = '\0';
  if (typeValue.first == REQUEST)
    clientReq->filemeta.name = value;
  typeValue.second = value;

  delete msg;
  delete value;
  return typeValue;
}

bool fileExists(FileData* file)
{
  FILE* pFile;
  char* fileData;

  pFile = fopen(file->name.c_str(), "rb");
  if (pFile == NULL)
    return false;

  fseek(pFile, 0, SEEK_END);
  file->length = ftell(pFile);
  rewind(pFile);
  fileData = (char*)malloc((file->length + 1) * sizeof(char));
  fread(fileData, 1, file->length, pFile);
  file->content = fileData;

  free(fileData);
  fclose(pFile);
  return true;
}

void createSegments()
{
  int curSequence, curFilePos, dataLen, fileSize;
  string header, segmentData;
  Ack segment;

  curSequence = curFilePos = 0;
  fileSize = clientReq->filemeta.length;
  
  if (fileSize == 0)
  {
    header = "SEQ: " + to_string(curSequence) + "\n" +
             "File Size: " + to_string(fileSize) + "B\n" + 
             "CWND: " + to_string(clientReq->sequenceSpace.cwnd) + "B\n\n";
    segmentData = header;

    segment.sequence = curSequence;
    segment.data = segmentData;
    clientReq->sequenceSpace.seqNums.push_back(segment);
    return;
  }

  while (curFilePos < fileSize)
  {
    header = "SEQ: " + to_string(curSequence) + "\n" +
             "File Size: " + to_string(fileSize) + "B\n" + 
             "CWND: " + to_string(clientReq->sequenceSpace.cwnd) + "B\n\n";
    dataLen = MAX_PACKET_SIZE - header.size();
    segmentData = header +
                  clientReq->filemeta.content.substr(curFilePos, dataLen);

    segment.sequence = curSequence;
    segment.data = segmentData;
    clientReq->sequenceSpace.seqNums.push_back(segment);
    
    curSequence = (curSequence + segmentData.size()) % (MAX_SEQUENCE + 1);
    curFilePos += dataLen;
  }

  return;
}

void processAck(AckSpace* sequenceSpace, int n)
{
  int i, j, windowSize;
  list<pair<int, struct timeval> >::iterator it;

  windowSize = sequenceSpace->windowSize;

  for (i = 0, j = sequenceSpace->base; i < windowSize; j++)
  {
    if (sequenceSpace->seqNums[j].sequence == n)
    {
      it = sequenceSpace->sentUnacked.begin();
      while (it != sequenceSpace->sentUnacked.end())
      {
        // Remove sequence from sentUnacked list
        if (sequenceSpace->seqNums[it->first].sequence == n)
        {
          if (it == sequenceSpace->sentUnacked.begin())
          {
            sequenceSpace->sentUnacked.erase(it);
            timerSet = false;
            checkTimeout();
          }
          else
            sequenceSpace->sentUnacked.erase(it);
          break;
        }
        it++;
      }
      sequenceSpace->seqNums[j].isAcked = true;
      fprintf(stdout, "* ACK %d processed\n", n);
      if (PRINT_WINDOW)
        print_window(sequenceSpace->base, 4, 0, true);

      /* Slide window if packet ACKed is at the base */
      if (j == sequenceSpace->base)
      {
        while (sequenceSpace->base < (int)sequenceSpace->seqNums.size() &&
               sequenceSpace->seqNums[sequenceSpace->base].isAcked)
        {
          sequenceSpace->windowSize -=
            sequenceSpace->seqNums[sequenceSpace->base].data.size();
          sequenceSpace->base++;
        }
        if (PRINT_WINDOW)
          print_window(sequenceSpace->base, 4, 0, true);
      }
      break;
    }
    i += sequenceSpace->seqNums[j].data.size();
  }

  return;
}

void sendPackets(AckSpace* sequenceSpace, int sockfd,
                 struct sockaddr* destAddr, socklen_t destLen)
{
  int i, n, diff;
  struct timeval now;
  struct itimerval timeout;
  string curPacket;
  pair<int, struct timeval> indexTime;

  while (sequenceSpace->windowSize < sequenceSpace->cwnd &&
         sequenceSpace->nextSeq < (int)sequenceSpace->seqNums.size())
  {
    i = sequenceSpace->nextSeq;
    curPacket = sequenceSpace->seqNums[i].data;
    diff = sequenceSpace->cwnd - sequenceSpace->windowSize;

    if ((int)curPacket.size() > diff)
      break;
    n = sendMsg(sockfd, curPacket.c_str(), curPacket.size(), 0, destAddr,
                destLen);
    if (n < 0)
    {
      perror("* ERROR sending to client address");
      exit(1);
    }
    gettimeofday(&now, NULL);
    indexTime = make_pair(i, now);
    sequenceSpace->sentUnacked.push_back(indexTime);
    if (!timerSet)
    {
      timerSet = true;
      timeout.it_interval.tv_sec = timeout.it_interval.tv_usec = 0;
      timeout.it_value.tv_sec = 0;
      timeout.it_value.tv_usec = ACK_TIMEOUT * 1E3;
      if (setitimer(ITIMER_REAL, &timeout, NULL) == -1)
      {
        perror("ERROR setting timer");
        exit(1);
      }
      //fprintf(stdout, "* Timer (%ld ms) set for SEQ %d!\n", ACK_TIMEOUT,
        //sequenceSpace->seqNums[i].sequence);
    }
    fprintf(stdout, "* %s SEQ %d (%d B) sent\n",
      get_time().c_str(), sequenceSpace->seqNums[i].sequence, n);

    sequenceSpace->windowSize += curPacket.size();
    sequenceSpace->nextSeq++;
  }
  return;
}

void catchAlarm(int signal)
{
  timerSet = false;
  timeout_flag = 1;
  checkTimeout();
  
}

void alertTimeout()
{
  vector<pair<int, string> >::iterator it;

  it = clientReq->sequenceSpace.timedOut.begin();

  if (it != clientReq->sequenceSpace.timedOut.end())
    fprintf(stdout, "*\n");
  else
    return;
  
  while (it != clientReq->sequenceSpace.timedOut.end())
  {
    fprintf(stdout, "* %s SEQ %d has been resent due to timeout!\n",
      it->second.c_str(),
      clientReq->sequenceSpace.seqNums[it->first].sequence);
    it = clientReq->sequenceSpace.timedOut.erase(it);
  }
  fprintf(stdout, "*\n");
  timeout_flag = 0;
  return;
}

void checkTimeout()
{
  long diff;
  int i, n, index, sockfd;
  struct timeval now;
  struct itimerval timeout;
  struct sockaddr* destAddr;
  socklen_t destLen;
  string packetData;
  vector<pair<int, struct timeval> > resent;
  list<pair<int, struct timeval> >::iterator it;

  it = clientReq->sequenceSpace.sentUnacked.begin();
  sockfd = clientReq->clientInfo.sockfd;
  destAddr = clientReq->clientInfo.address;
  destLen = clientReq->clientInfo.length;

  while (it != clientReq->sequenceSpace.sentUnacked.end())
  {
    index = it->first;
    packetData = clientReq->sequenceSpace.seqNums[index].data;

    gettimeofday(&now, NULL);
    diff = 
      (1E3 * (now.tv_sec - it->second.tv_sec)) +
      (1E-3 * (now.tv_usec - it->second.tv_usec));

    if (diff >= ACK_TIMEOUT) // Resend packet
    {
      clientReq->sequenceSpace.timedOut.push_back(
        make_pair(index, get_time())
      );
      n = sendMsg(sockfd, packetData.c_str(), packetData.size(), 0, destAddr,
                  destLen);
      if (n < 0)
      {
        perror("ERROR sending to client address");
        exit(1);
      }

      it = clientReq->sequenceSpace.sentUnacked.erase(it);
      gettimeofday(&now, NULL);
      resent.push_back(make_pair(index, now));
      continue;
    }
    else if (!timerSet)
    {
      timerSet = true;
      timeout.it_interval.tv_sec = timeout.it_interval.tv_usec = 0;
      timeout.it_value.tv_sec = 0;
      timeout.it_value.tv_usec = (ACK_TIMEOUT - diff) * 1E3;
      if (setitimer(ITIMER_REAL, &timeout, NULL) == -1)
      {
        perror("ERROR setting timer");
        exit(1);
      }
      break;
    }

    it++;
  }

  /* For any packet that timed out and got resent, push those to the back
   * of the queue */
  for (i = 0; i < (int)resent.size(); i++)
  {
    clientReq->sequenceSpace.sentUnacked.push_back(resent[i]);
    if (!timerSet)
    {
      timerSet = true;
      timeout.it_interval.tv_sec = timeout.it_interval.tv_usec = 0;
      timeout.it_value.tv_sec = 0;
      timeout.it_value.tv_usec = ACK_TIMEOUT * 1E3;
      if (setitimer(ITIMER_REAL, &timeout, NULL) == -1)
      {
        perror("ERROR setting timer");
        exit(1);
      }
    }
  }

  return;
}

void freeClient(int signal)
{
  if (clientReq != NULL)
    delete clientReq;
  exit(0);
}

string get_time()
{
  struct timeval tv;
  time_t long_time;
  struct tm *newtime;
  char result[128] = {0};
  string timeRes;

  gettimeofday(&tv,0);
  time(&long_time);
  newtime = localtime(&long_time);
  
  sprintf(result, "(%02d:%02d:%02d.%03ld)", newtime->tm_hour,
    newtime->tm_min, newtime->tm_sec, (long)tv.tv_usec / 1000);
  timeRes = result;
  return timeRes;
}

void print_window(int base, int n, int init, bool isFirst)
{
  int i, j, k, m, len, windowSize, sequence, nSeq;
  string seqLine;
  ostringstream lines[5];

  windowSize = clientReq->sequenceSpace.cwnd;
  j = base;
  nSeq = 0;

  // Beginning
  if (isFirst)
  {
    fprintf(stdout, "*\n");
    fprintf(stdout, "* %d/%d bytes used in window\n",
      clientReq->sequenceSpace.windowSize, windowSize);
  }

  if (j != 0)
  {
    lines[0] << "*-------";
    lines[1] << "*       ";
    lines[2] << "*  ...  ";
    lines[3] << "*       ";
    lines[4] << "*-------";
  }

  if (j == (int)clientReq->sequenceSpace.seqNums.size())
    j--; // Show last sequence ACKed

  for (i = init;
       nSeq < n && j < (int)clientReq->sequenceSpace.seqNums.size() &&
       i < windowSize; j++)
  {
    sequence = clientReq->sequenceSpace.seqNums[j].sequence;
    seqLine = "|  " + to_string(sequence) + "  ";
    len = seqLine.size() - 5;

    for (k = 0; k < 5; k++)
    {
      if (k == 2)
        lines[k] << seqLine;
      else if (k == 0 || k == 4)
      {
        lines[k] << "*--";
        for (m = 0; m < len; m++)
          lines[k] << "-";
        lines[k] << "--";
      }
      else if (k == 1)
      {
        if (len == 1)
          lines[k] << "| SEQ ";
        else if (len % 2 == 0)
        {
          if (n != 2)
            lines[k] << "|  ";
          for (m = 0; m < (len / 2) - 2; m++)
            lines[k] << " ";
          lines[k] << "SEQ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          lines[k] << "  ";
        }
        else
        {
          lines[k] << "|  ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          lines[k] << "SEQ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          lines[k] << "  ";
        }
      }
      else // k == 3
      {
        if (len == 1)
        {
          if (clientReq->sequenceSpace.seqNums[j].isAcked)
            lines[k] << "| ACK ";
          else
            lines[k] << "|     ";
        }
        else if (len % 2 == 0)
        {
          if (len != 2)
            lines[k] << "|  ";
          for (m = 0; m < (len / 2) - 2; m++)
            lines[k] << " ";
          if (clientReq->sequenceSpace.seqNums[j].isAcked)
            lines[k] << "ACK";
          else
            lines[k] << "   ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          lines[k] << "  ";
        }
        else
        {
          lines[k] << "|  ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          if (clientReq->sequenceSpace.seqNums[j].isAcked)
            lines[k] << "ACK";
          else
            lines[k] << "   ";
          for (m = 0; m < (len / 2) - 1; m++)
            lines[k] << " ";
          lines[k] << "  ";
        }
      }
    }

    nSeq++;
    i += clientReq->sequenceSpace.seqNums[j].data.size();
  }

  if (j != (int)clientReq->sequenceSpace.seqNums.size())
  {
    lines[0] << "*-------*";
    lines[1] << "|       |";
    lines[2] << "|  ...  |";
    lines[3] << "|       |";
    lines[4] << "*-------*";
  }
  else
  {
    lines[0] << "*";
    lines[1] << "|";
    lines[2] << "|";
    lines[3] << "|";
    lines[4] << "*";
  }

  for (m = 0; m < 5; m++)
    fprintf(stdout, "%s\n", lines[m].str().c_str());

  if (i < windowSize && j < (int)clientReq->sequenceSpace.seqNums.size())
    print_window(j, n, i, false);
  if (isFirst)
    fprintf(stdout, "*\n");
  return;
}
