//
// Created by Rut Vora
//

#include "Sender.h"
#include "Receiver.h"

UnshapedTransciever::Receiver *receiver;
UnshapedTransciever::Sender *sender;

int clientSocket;

ssize_t receivedData(int fromSocket, std::string &clientAddress,
                     uint8_t *buffer, size_t length) {
  (void) (clientAddress);
  clientSocket = fromSocket;
  return sender->sendData(buffer, length);
}

ssize_t sendData(UnshapedTransciever::Sender *receivedResponseFrom,
                 uint8_t *buffer, size_t length) {
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


  receiver = new UnshapedTransciever::Receiver{"", 8000, receivedData};
  sender = new UnshapedTransciever::Sender{remoteHost, remotePort, sendData};

  receiver->startListening();

  std::string str;
  std::cin >> str;
  return 0;
}