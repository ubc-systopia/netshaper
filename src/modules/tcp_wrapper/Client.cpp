//
// Created by ubuntu on 12/29/22.
//

#include "Client.h"

#include <utility>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sstream>
#include <thread>
#include <cstring>
#include <iomanip>

namespace TCP {
  Client::Client(const std::string &remoteHost, int remotePort,
                 std::function<void(TCP::Client *,
                                    uint8_t *buffer, size_t length,
                                    connectionStatus connStatus)>
                 onReceiveFunc, logLevels level)
      : logLevel(level), remoteSocket(-1) {
    onReceive = std::move(onReceiveFunc);

    this->remoteHost = remoteHost;
    this->remotePort = remotePort;

    int error = connectToRemote();
    if (error < 0) {
      close(remoteSocket);
      switch (error) {
        case CLIENT_RESOLVE_ERROR:
          throw std::runtime_error("Could not resolve " + remoteHost + ":" +
                                   std::to_string(remotePort));
        case CLIENT_SOCKET_ERROR:
          throw std::runtime_error("Could not open socket to " + remoteHost +
                                   ":" +
                                   std::to_string(remotePort));
        case CLIENT_CONNECT_ERROR:
          throw std::runtime_error("Could not connect to " + remoteHost + ":" +
                                   std::to_string(remotePort));
        default:
          throw std::runtime_error("Unhandled Exception");
      }
    }

    std::thread receive(&Client::startReceiving, this);
    receive.detach();
#ifdef DEBUGGING
    log(DEBUG, "Client initialised");
#endif
  }

  Client::~Client() {
    close(remoteSocket);
#ifdef DEBUGGING
    log(DEBUG, "Client at socket: " + std::to_string(remoteSocket)
               + " destructed");
#endif
  }

  int Client::connectToRemote() {
    struct addrinfo hints{}, *res = nullptr;

    // getaddrinfo requires the port in the c_string format
    char portString[6];
    sprintf(portString, "%d", remotePort);

    hints.ai_flags = AI_NUMERICSERV; // numeric service (port) number
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(remoteHost.c_str(), portString, &hints, &res) != 0) {
      errno = EFAULT;
      return CLIENT_RESOLVE_ERROR;
    }

    if ((remoteSocket = socket(res->ai_family, res->ai_socktype,
                               res->ai_protocol)
        ) <
        0) {
      return CLIENT_SOCKET_ERROR;
    }

    if (connect(remoteSocket, res->ai_addr, res->ai_addrlen) < 0) {
      return CLIENT_CONNECT_ERROR;
    }

    if (res != nullptr) {
      freeaddrinfo(res);
    }

#ifdef DEBUGGING
    std::stringstream ss;
    ss << "Connected to remote address " << remoteHost << ":" << remotePort <<
       " at socket " << remoteSocket;
    log(DEBUG, ss.str());
#endif
    return 0;
  }

  void Client::startReceiving() {
    ssize_t bytesReceived;  // Number of bytes received
    uint8_t buffer[BUF_SIZE];

    // Read from fromSocket and send to toSocket
    while ((bytesReceived = recv(remoteSocket, buffer, BUF_SIZE, 0)) > 0) {
#ifdef DEBUGGING
      std::stringstream ss;
      ss << "Received data on socket: " << remoteSocket;
      log(DEBUG, ss.str());
#endif
      onReceive(this, buffer, bytesReceived, ONGOING);
    }

    if (bytesReceived < 0) {
      log(ERROR, "Server at " + remoteHost + ":" + std::to_string(remotePort) +
                 " disconnected abruptly with error " + strerror(errno));
    }
    onReceive(this, nullptr, 0, FIN);
    shutdown(remoteSocket, SHUT_RD);
  }

  ssize_t Client::sendData(uint8_t *buffer, size_t length) {
#ifdef DEBUGGING
    std::stringstream ss;
    ss << "Sending data to socket: " << remoteSocket;
    log(DEBUG, ss.str());
#endif
    auto bytesSent = send(remoteSocket, buffer, length, 0);
    return bytesSent;
  }

  void Client::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "TcpClient:DEBUG: ";
        break;
      case ERROR:
        levelStr = "TcpClient:ERROR: ";
        break;
      case WARNING:
        levelStr = "TcpClient:WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }
}
