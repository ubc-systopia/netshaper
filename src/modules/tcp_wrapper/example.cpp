//
// Created by Rut Vora
//

#include "Client.h"
#include "Server.h"
#include <cstring>

TCP::Server *server;
TCP::Client *client;

int clientSocket;

bool serverCallback(int fromSocket, std::string &clientAddress,
                    uint8_t *buffer, size_t length, enum
                        connectionStatus connStatus) {
  (void) (clientAddress);
  clientSocket = fromSocket;
  if (connStatus != ONGOING) return true;
  std::cout << "Server callback..." << std::endl;
  return client->sendData(buffer, length) > 0;
}

ssize_t clientCallback(TCP::Client *receivedResponseFrom,
                       uint8_t *buffer, size_t length,
                       connectionStatus connStatus) {
  (void) (connStatus);
  (void) (receivedResponseFrom);
  return server->sendData(clientSocket, buffer, length);
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


  server = new TCP::Server{"", 8000, serverCallback};
  client = new TCP::Client{remoteHost, remotePort, clientCallback};

  server->startListening();

  std::string str;
  std::cin >> str;
  return 0;
}