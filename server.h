#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <sys/time.h>
#include <string>
#include <vector>
#include <utility>
#include <list>

enum MessageType { REQUEST, ACK, UNKNOWN };

/* Information about the file requested by the client */
struct FileData
{
  FileData();
  std::string name;
  int         length;
  std::string content;
};

/* Client information */
struct Client
{
  struct sockaddr* address;
  socklen_t        length;
  int              sockfd;
};

/* Packet information */
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

  /* Current window size used */
  int windowSize;

  /* Bytes ACKed in current window excluding first packet in window */
  int windowBuffer;

  /* Window size */
  int cwnd;

  /* Points to first index of current window */
  int base;

  /* Points to index of next segment to be sent */
  int nextSeq;

  /* Container of indexes of packets sent but un-ACKed */
  std::list<std::pair<int, struct timeval> > sentUnacked;
};

/* Contains information about the client's request */
struct SRInfo
{
  FileData filemeta;
  Client   clientInfo;
  AckSpace sequenceSpace;
};

/**
 * Receive a message from a socket
 * @param sockfd[IN] socket file descriptor used in recvfrom function call
 * @param msg[OUT] string where contents are stored
 * @param flags[IN] type of message reception
 * @param clientAddr[OUT] null pointer, or points to a sockaddr structure in
 *                        which the sending address is to be stored
 * @param addrLen[IN] length of the sockaddr pointed to by clientAddr
 * @return total number of bytes read. 0 if no messages and client
 *         disconnected. Otherwise, -1 and errno is set
 */
int recvMsg(int sockfd, std::string& msg, int flags,
            struct sockaddr* clientAddr, socklen_t* addrLen);

/**
 * Sends a message through a socket
 * @param sockfd[IN] socket file descriptor used in sendto function call
 * @param buffer[IN] points to a buffer contaning the message to be sent
 * @param length[IN] size of the message in bytes
 * @param flags[IN] type of message transmission
 * @param destAddr[IN] points to a sockaddr structure containing the
 *                     destination address
 * @param destLen[IN] length of the sockaddr structure pointed to by destAddr
 * @return number of bytes sent upon success. Otherwise, -1 and errno is set
 */
int sendMsg(int sockfd, const void* buffer, size_t length, int flags,
            struct sockaddr* destAddr, socklen_t destLen);

/**
 * Parses the message received from the client
 * @param message[IN] message received from the client
 * @return pair containing the message type (REQUEST, ACK, or UNKNOWN) and
 *         its corresponding value (REQUEST: file name, ACK: sequence
 *         number, UNKNOWN: empty)
 */
std::pair<MessageType, std::string> parseMsg(std::string message);

/**
 * Loads the content and length of the indicated file into the FileData
 * structure (if it exists)
 * @param file[IN/OUT] points to a FileData structure containing the name of
 *                     the file
 * @return true if the file exists. Otherwise, false.
 */
bool fileExists(FileData* file);

/*
 * Creates segments (packets) based on the file requested and loads them into
 * the SRInfo structure
 */
void createSegments();

/**
 * Processes an ACK packet by marking it as ACKed and decreasing the window
 * size used
 * @param sequenceSpace[IN/OUT] points to an AckSpace structure containing the
 *                              packets to be marked as ACKed
 * @param n[IN] ACK sequence number
 */
void processAck(AckSpace* sequenceSpace, int n);

/**
 * Sends packets if there is space in the current window
 * @param sequenceSpace[IN/OUT] points to an AckSpace structure containing the
 *                              packets
 * @param sockfd[IN] socket file descriptor used in sendto function call
 * @param destAddr[IN] points to a sockaddr structure containing the
 *                     destination address
 * @param destLen[IN] length of the sockaddr structure pointed to by destAddr
 */
void sendPackets(AckSpace* sequenceSpace, int sockfd,
                 struct sockaddr* destAddr, socklen_t destLen);

/**
 * Handler function for SIGALRM that calls checkTimeout and resets the alarm
 * @param signal[IN] signal value
 */
void catchAlarm(int signal);

/**
 * Check if any packets that were sent but not yet ACKed have gone over the
 * time limit (timeout). Resend if so.
 */
void checkTimeout();

/**
 * Outputs the current time in hours, minutes, seconds, and milliseconds.
 */
void print_time();

/* Global variables */
const int BUFFER_SIZE = 1024;
const int TIMEOUT = 4;
const int MAX_PACKET_SIZE = 1024;                   // Unit of bytes
const int MAX_SEQUENCE = 30 * MAX_PACKET_SIZE;      // Unit of bytes
const suseconds_t ACK_TIMEOUT = (200 * 1E-3) * 1E6; // Unit of microseconds
const unsigned ALARM_TIME = 5;                      // Unit of seconds

SRInfo* clientReq;

#endif /* SERVER_H */
