//
// Created by Rut Vora
//

#include "Receiver.h"

#include <utility>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <thread>
#include <cstring>
#include <iomanip>

#ifdef RECORD_STATS
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut;
extern std::vector<std::vector<uint64_t>> tcpSend;
#endif

namespace TCP {
  Receiver::Receiver(std::string bindAddr, int localPort,
                     std::function<bool(int fromSocket,
                                        std::string &clientAddress,
                                        uint8_t *buffer, size_t length,
                                        enum connectionStatus connStatus)>
                     onReceiveFunc,
                     logLevels level) : logLevel(level) {
    if (bindAddr.empty()) bindAddr = "0.0.0.0";
    inetFamily = checkIPVersion(bindAddr);
    if (inetFamily == -1) {
      throw std::invalid_argument(
          "bindAddr should be IP addresses only");
    }
    this->bindAddr = std::move(bindAddr);
    this->localPort = localPort;
    onReceive = std::move(onReceiveFunc);

    // Initialise localSocket to -1 (invalid value)
    this->localSocket = -1;
#ifdef DEBUGGING
    log(DEBUG, "Receiver initialised");
#endif
  }

  Receiver::~Receiver() {
    close(localSocket);
#ifdef DEBUGGING
    log(DEBUG, "Receiver destructed");
#endif
    exit(0);
  }

  void Receiver::startListening() {
    localSocket = openSocket();
    switch (localSocket) {
      case CLIENT_RESOLVE_ERROR:
        throw std::invalid_argument("Could not resolve the given bind Address "
                                    "and port");
      case SERVER_SOCKET_ERROR:
        throw std::runtime_error("Could not open the socket (do you have "
                                 "permission?). Port < 1024 require sudo");
      case SERVER_SETSOCKOPT_ERROR:
        throw std::runtime_error("Could not set the socket options");
      case SERVER_BIND_ERROR:
        throw std::runtime_error("Could not bind to given socket (maybe it is "
                                 "in use?)");
      case SERVER_LISTEN_ERROR:
        throw std::runtime_error("Could not start listening on given address "
                                 "and port");
      default:
#ifdef DEBUGGING
        log(DEBUG, "Started listening on " + bindAddr + ":"
                   + std::to_string(localPort));
#endif
        std::thread loop(&Receiver::serverLoop, this);
        loop.detach();
    }

  }

  int Receiver::openSocket() {
    struct addrinfo hints = {}, *res = nullptr;

    // getaddrinfo(...) requires the port in the c_string format
    char portString[6];
    sprintf(portString, "%d", localPort);

    if (!bindAddr.empty()) {
      /* check for numeric IP to specify IPv6 or IPv4 socket */
      int validFamily = checkIPVersion(bindAddr);
      if (validFamily != -1) {
        hints.ai_family = validFamily;
        hints.ai_flags |= AI_NUMERICHOST; /* bind_addr is a valid numeric ip, skip resolve */
      }
    } else {
      /* if bind_address is NULL, will bind to IPv6 wildcard */
      hints.ai_family = AF_INET6; /* Specify IPv6 socket, also allow ipv4 clients */
      hints.ai_flags |= AI_PASSIVE; /* Wildcard address */
    }

    if (getaddrinfo(bindAddr.c_str(), portString, &hints, &res) != 0) {
      return CLIENT_RESOLVE_ERROR;
    }

    int serverSocket = socket(res->ai_family, res->ai_socktype,
                              res->ai_protocol);
    if (serverSocket < 0) {
      return SERVER_SOCKET_ERROR;
    }

    int optVal = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof
        (optVal)) < 0) {
      return SERVER_SETSOCKOPT_ERROR;
    }

    if (bind(serverSocket, res->ai_addr, res->ai_addrlen) == -1) {
      close(serverSocket);
      return SERVER_BIND_ERROR;
    }

    if (listen(serverSocket, BACKLOG) < 0) {
      return SERVER_LISTEN_ERROR;
    }

    if (res != nullptr) {
      freeaddrinfo(res);
    }
#ifdef DEBUGGING
    log(DEBUG, "Receiver running on socket " + std::to_string(serverSocket));
#endif
    return serverSocket;

  }

  // Returns a string of the form "address:port"
  std::string Receiver::getAddress(struct sockaddr &sockAddr) {
    if (sockAddr.sa_family == AF_INET) {
      auto addrStruct = reinterpret_cast<struct sockaddr_in *>(&sockAddr);
      char addr[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &addrStruct->sin_addr, addr, INET_ADDRSTRLEN) ==
          nullptr)
        throw std::domain_error("Could not parse address");
      return
          std::string(addr) + ":" + std::to_string(ntohs(addrStruct->sin_port));
    } else if (sockAddr.sa_family == AF_INET6) {
      auto addrStruct = reinterpret_cast<struct sockaddr_in6 *>(&sockAddr);
      char addr[INET6_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &addrStruct->sin6_addr, addr, INET6_ADDRSTRLEN) ==
          nullptr)
        throw std::domain_error("Could not parse address");
      return
          std::string(addr) + ":" +
          std::to_string(ntohs(addrStruct->sin6_port));
    } else {
      return "";
    }
  }

  [[noreturn]] void Receiver::serverLoop() {
    struct sockaddr_storage clientAddress{};
    socklen_t addrLen = sizeof(clientAddress);
#ifdef DEBUGGING
    log(DEBUG, "Starting Receiver loop");
#endif
    while (true) {
      int clientSocket =
          accept(localSocket, (struct sockaddr *) &clientAddress,
                 &addrLen);


      try {
        auto address = getAddress(*(struct sockaddr *) (&clientAddress));
        std::thread clientHandler(&Receiver::handleClient, this, clientSocket,
                                  address);
        clientHandler.detach();
      } catch (...) {
        log(ERROR, "Could not parse client address");
      }
    }

  }

  void Receiver::handleClient(int clientSocket, std::string clientAddress) {
#ifdef DEBUGGING
    log(DEBUG, "Client at " + clientAddress + " connected on socket " +
               std::to_string(clientSocket));
#endif
    if (!onReceive(clientSocket, clientAddress, nullptr, 0, SYN)) {
      // No queues for this client. Don't receive data from it
      close(clientSocket);
      return;
    }
    // Read data from the client socket
    receiveData(clientSocket, clientAddress);
  }

  void Receiver::receiveData(int socket, std::string &clientAddress) {
    ssize_t bytesReceived;  // Number of bytes received
    uint8_t buffer[BUF_SIZE];

    // Read from fromSocket and send to toSocket
    while ((bytesReceived = recv(socket, buffer, BUF_SIZE, 0)) > 0) {
#ifdef RECORD_STATS
      tcpIn[socket].push_back(std::chrono::steady_clock::now());
#endif
#ifdef DEBUGGING
      log(DEBUG, "Data received on socket " + std::to_string(socket));
#endif
      onReceive(socket, clientAddress, buffer, bytesReceived, ONGOING);
    }

    if (bytesReceived < 0) {
      log(ERROR, "Client at " + clientAddress + " disconnected abruptly with "
                                                "error " + strerror(errno));
    }
    // Stop other processes from using these sockets
#ifdef DEBUGGING
    log(DEBUG, "Shutting down read on socket " + std::to_string(socket));
#endif
    shutdown(socket, SHUT_RD);
    onReceive(socket, clientAddress, nullptr, 0, FIN);
  }

  ssize_t Receiver::sendData(int toSocket, uint8_t *buffer, size_t length) {
#ifdef DEBUGGING
    log(DEBUG, "Sending data on socket " + std::to_string(toSocket));
#endif
#ifdef RECORD_STATS
    tcpOut[toSocket].push_back(std::chrono::steady_clock::now());
    auto start = std::chrono::steady_clock::now();
    auto bytesSent = send(toSocket, buffer, length, 0);
    auto end = std::chrono::steady_clock::now();
    tcpSend[toSocket].push_back((end - start).count());
#else
    auto bytesSent = send(toSocket, buffer, length, 0);
#endif
    return bytesSent;
  }

  int Receiver::checkIPVersion(const std::string &address) {
/* Check for valid IPv4 or Iv6 string. Returns AF_INET for IPv4, AF_INET6 for IPv6 */

    struct in6_addr addr{};
    const char *addressCString = address.c_str();
    if (inet_pton(AF_INET, addressCString, &addr) == 1) {
      return AF_INET;
    } else {
      if (inet_pton(AF_INET6, addressCString, &addr) == 1) {
        return AF_INET6;
      }
    }

    log(ERROR, "Wrong bind address entered. Need IPv4 or IPv6 address");
    return -1;
  }

  void Receiver::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "TR:DEBUG: ";
        break;
      case ERROR:
        levelStr = "TR:ERROR: ";
        break;
      case WARNING:
        levelStr = "TR:WARNING: ";
        break;
    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }
}