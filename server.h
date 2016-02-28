#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <string>
#include <vector>
#include <utility>

const int BUFFER_SIZE = 1024;
const int TIMEOUT = 4;
const int MAX_PACKET_SIZE = 1024;              // Unit of bytes
const int MAX_SEQUENCE = 30 * MAX_PACKET_SIZE; // Unit of bytes

enum MessageType { REQUEST, ACK, UNKNOWN };

struct FileData
{
  FileData();
  std::string name;
  int         length;
  std::string content;
};

struct Ack
{
  Ack();
  int         sequence;
  bool        isAcked;
  std::string data;
};

/* Server's view of sequence numbers */
struct AckSpace
{
  AckSpace();
  std::vector<Ack> seqNums;
  int              windowSize; // Current size used
  int              base;       // Points to first index of current window
  int              nextSeq;    // Points to index of next segment to be sent
};

/* Contains information about the client's request */
struct SRInfo
{
  FileData filemeta;
  AckSpace sequenceSpace;
};

int getClientMsg(int sockfd, std::string& msg, int flags,
                 struct sockaddr* clientAddr, socklen_t* addrLen);

int sendMsg(int sockfd, const void* buffer, size_t length, int flags,
            struct sockaddr* destAddr, socklen_t destLen);

std::pair<MessageType, std::string> parseMsg(std::string message,
                                             SRInfo* clientRequest);

bool fileExists(FileData* file);

void createSegments(SRInfo* clientRequest);

void processAcks(AckSpace* sequenceSpace, std::string acks);

void sendPackets(AckSpace* sequenceSpace, int windowSize, int sockfd,
                 struct sockaddr* destAddr, socklen_t destLen);

#endif /* SERVER_H */
