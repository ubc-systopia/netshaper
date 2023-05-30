//
// Created by Rut Vora
//

#include "Client.h"
#include "Server.h"
#include <cstring>
#include <shared_mutex>

ssize_t clientCallback(TCP::Client *client,
                       uint8_t *buffer, size_t length,
                       connectionStatus connStatus);

TCP::Server *server;
std::string remoteHost;
int remotePort = 0;

std::unordered_map<int, TCP::Client *> socketToClient{};
std::unordered_map<TCP::Client *, int> clientToSocket{};

std::shared_mutex mapLock;

bool serverCallback(int fromSocket, std::string &clientAddress,
                    uint8_t *buffer, size_t length, enum
                        connectionStatus connStatus) {
  (void) (clientAddress);
  if (connStatus == SYN) {
    // A new client joined...
    auto client = new TCP::Client{remoteHost, remotePort, clientCallback};
    mapLock.lock();
    socketToClient[fromSocket] = client;
    clientToSocket[client] = fromSocket;
    mapLock.unlock();
    return true;
  } else if (connStatus == FIN) {
    mapLock.lock();
    auto client = socketToClient[fromSocket];
    clientToSocket.erase(client);
    socketToClient.erase(fromSocket);
    client->sendFIN();
    mapLock.unlock();
    return true;
  } else {
    return socketToClient[fromSocket]->sendData(buffer, length) > 0;
  }
}

ssize_t clientCallback(TCP::Client *client,
                       uint8_t *buffer, size_t length,
                       connectionStatus connStatus) {
  (void) (connStatus);
  mapLock.lock_shared();
  auto retVal = server->sendData(clientToSocket[client], buffer, length);
  mapLock.unlock_shared();
  return retVal;
}

int main() {
  std::cout << "Enter the IP address of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remoteHost;

  std::cout << "Enter the port of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remotePort;


  server = new TCP::Server{"", 8000, serverCallback};

  server->startListening();

  std::string str;
  std::cin >> str;
  return 0;
}