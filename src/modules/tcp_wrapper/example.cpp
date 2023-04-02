//
// Created by Rut Vora
//

#include "Sender.h"
#include "Receiver.h"
#include <string.h>

TCP::Receiver *receiver;
TCP::Sender *sender;

int clientSocket;

bool receiverCallback(int fromSocket, std::string &clientAddress,
                      uint8_t *buffer, size_t length, enum
                          connectionStatus connStatus) {
  (void) (clientAddress);
  clientSocket = fromSocket;
  if (connStatus != ONGOING) return true;
  std::cout << "Receiver callback..." << std::endl;
  return sender->sendData(buffer, length) > 0;
}

ssize_t senderCallback(TCP::Sender *receivedResponseFrom,
                       uint8_t *buffer, size_t length,
                       connectionStatus connStatus) {
  (void) (connStatus);
  (void) (receivedResponseFrom);
  return receiver->sendData(clientSocket, buffer, length);
}

int main() {
  std::string remoteHost;
  int remotePort;

  std::cout << "Enter the IP address of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remoteHost;

  std::cout << "Enter the port of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remotePort;


  receiver = new TCP::Receiver{"", 8000, receiverCallback};
  sender = new TCP::Sender{remoteHost, remotePort, senderCallback};

  receiver->startListening();

  std::string str;
  std::cin >> str;
  return 0;
}