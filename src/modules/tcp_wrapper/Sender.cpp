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
#include <iomanip>

extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut;

namespace TCP {
  Sender::Sender(const std::string &remoteHost, int remotePort,
                 std::function<void(TCP::Sender *,
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
      tcpIn[remoteSocket].push_back(std::chrono::steady_clock::now());
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
    tcpOut[remoteSocket].push_back(std::chrono::steady_clock::now());
    return send(remoteSocket, buffer, length, 0);
  }

  void Sender::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "TS:DEBUG: ";
        break;
      case ERROR:
        levelStr = "TS:ERROR: ";
        break;
      case WARNING:
        levelStr = "TS:WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }
}