//
// Created by Rut Vora
//

#include "Sender.h"
#include "Receiver.h"

Receiver *receiver;
Sender *sender;

int clientSocket;

ssize_t receivedData(int fromSocket, uint8_t *buffer, size_t length) {
  clientSocket = fromSocket;
  return sender->sendData(buffer, length);
}

ssize_t sendData(uint8_t *buffer, size_t length) {
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


  receiver = new Receiver{"", 8000, receivedData};
  sender = new Sender{remoteHost, remotePort, sendData};

  receiver->startListening();

  std::string str;
  std::cin >> str;
  return 0;
}