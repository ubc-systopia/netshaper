//
// Created by ubuntu on 12/29/22.
//

#include "Sender.h"

#include <utility>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sstream>
#include <thread>
#include <cstring>

namespace TCP {
  Sender::Sender(std::string remoteHost, int remotePort,
                 std::function<void(TCP::Sender *,
                                    uint8_t *buffer, size_t length,
                                    connectionStatus connStatus)>
                 onReceiveFunc, logLevels level)
      : logLevel(level), remoteSocket(-1) {
    onReceive = std::move(onReceiveFunc);

    this->remoteHost = std::move(remoteHost);
    this->remotePort = remotePort;

    int error = connectToRemote();
    if (error < 0) {
      close(remoteSocket);
      switch (error) {
        case CLIENT_RESOLVE_ERROR:
          throw std::runtime_error("Could not resolve remote host");
        case CLIENT_SOCKET_ERROR:
          throw std::runtime_error("Could not open socket to remote host");
        case CLIENT_CONNECT_ERROR:
          throw std::runtime_error("Could not connect to remote host");
        default:
          throw std::runtime_error("Unhandled Exception");
      }
    }

    std::thread receive(&Sender::startReceiving, this);
    receive.detach();

    log(DEBUG, "Sender initialised");
  }

  Sender::~Sender() {
    close(remoteSocket);
    log(DEBUG, "Sender at socket: " + std::to_string(remoteSocket)
               + " destructed");
  }

  int Sender::connectToRemote() {
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

    std::stringstream ss;
    ss << "Connected to remote address " << remoteHost << ":" << remotePort <<
       " at socket " << remoteSocket;
    log(DEBUG, ss.str());
    return 0;
  }

  void Sender::startReceiving() {
    ssize_t bytesReceived;  // Number of bytes received
    uint8_t buffer[BUF_SIZE];

    // Read from fromSocket and send to toSocket
    while ((bytesReceived = recv(remoteSocket, buffer, BUF_SIZE, 0)) > 0) {
      std::stringstream ss;
      ss << "Received data on socket: " << remoteSocket;
      log(DEBUG, ss.str());
      onReceive(this, buffer, bytesReceived, ONGOING);
    }

    if (bytesReceived < 0) {
      log(ERROR, "Sender at " + remoteHost + ":" + std::to_string(remotePort) +
                 " disconnected abruptly with error " + strerror(errno));
    }
    onReceive(this, nullptr, 0, FIN);
    shutdown(remoteSocket, SHUT_RD);
  }

  ssize_t Sender::sendData(uint8_t *buffer, size_t length) {
    std::stringstream ss;
    ss << "Sending data to socket: " << remoteSocket;
    log(DEBUG, ss.str());
    return send(remoteSocket, buffer, length, 0);
  }

  void Sender::log(logLevels level, const std::string &log) {
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "DEBUG: ";
        break;
      case ERROR:
        levelStr = "ERROR: ";
        break;
      case WARNING:
        levelStr = "WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << levelStr << log << std::endl;
    }
  }
}