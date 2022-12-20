//
// Created by Rut Vora
//

#include "ProxyListener.h"

#include <utility>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>


ProxyListener::ProxyListener(std::string remoteHost, int remotePort,
                             std::string bindAddr, int localPort) {
  if (bindAddr.empty()) bindAddr = "0.0.0.0";
  inetFamily = checkIPVersion(bindAddr);
  int validRemote = checkIPVersion(remoteHost);
  if (inetFamily == -1 || validRemote == -1) {
    throw std::invalid_argument(
        "remoteHost and bindAddr should be IP addresses only");
  }
  this->bindAddr = std::move(bindAddr);
  this->localPort = localPort;
  this->remoteHost = std::move(remoteHost);
  this->remotePort = remotePort;

  // Initialise localSocket to -1 (invalid value)
  this->localSocket = -1;
}

ProxyListener::~ProxyListener() {
  close(localSocket);
  exit(0);
}

void ProxyListener::startListening() {
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
      std::cout << "Started Listening on " << bindAddr << ":" << localPort <<
                std::endl;
      serverLoop();
  }

}

int ProxyListener::openSocket() {
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

  return serverSocket;

}

void ProxyListener::serverLoop() {
  struct sockaddr_storage clientAddress{};
  socklen_t addrLen = sizeof(clientAddress);

  while (true) {
    int clientSocket = accept(localSocket, (struct sockaddr *) &clientAddress,
                              &addrLen);
    if (fork() == 0) {
      // Child process for handling the accepted connection
      close(localSocket); // Don't need child to listen to local socket
      handleClient(clientSocket);
      exit(0);
    }
    close(clientSocket);
  }

}

void ProxyListener::handleClient(int clientSocket) {
  int remoteSocket = connectToRemote();
  if (remoteSocket < 0) {
    close(remoteSocket);
    close(clientSocket);
    switch (remoteSocket) {
      case CLIENT_RESOLVE_ERROR:
        throw std::runtime_error("Could not resolve remote host");
      case CLIENT_SOCKET_ERROR:
        throw std::runtime_error("Could not open socket to remote host");
      case CLIENT_CONNECT_ERROR:
        throw std::runtime_error("Could not connect to remote host");
      default:
        throw std::runtime_error("Unhandled exception");
    }

  }

  if (fork() == 0) {
    // Child process
    // Forward data from clientSocket to remote socket
    forwardData(clientSocket, remoteSocket);
    exit(0);
  }

  if (fork() == 0) {
    // Child process
    // Forward data from remoteSocket to clientSocket
    forwardData(remoteSocket, clientSocket);
    exit(0);
  }
}

void ProxyListener::forwardData(int fromSocket, int toSocket) {
  ssize_t n;  // Number of bytes received
  char buffer[BUF_SIZE];

  // Read from fromSocket and send to toSocket
  while ((n = recv(fromSocket, buffer, BUF_SIZE, 0)) > 0) {
    send(toSocket, buffer, n, 0);
  }

  if (n < 0) {
    throw std::runtime_error("Broken pipe");
  }
  // Stop other processes from using these sockets
  shutdown(toSocket, SHUT_RDWR);
  close(toSocket);
  shutdown(fromSocket, SHUT_RDWR);
  close(fromSocket);
}

int ProxyListener::connectToRemote() {
  struct addrinfo hints{}, *res = nullptr;
  int validFamily;
  int remoteSocket;

  // getaddrinfo requires the port in the c_string format
  char portString[6];
  sprintf(portString, "%d", remotePort);

  hints.ai_flags = AI_NUMERICSERV; // numeric service (port) number
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  validFamily = checkIPVersion(remoteHost);
  if (validFamily != -1) {
    hints.ai_family = validFamily;

    // Setting this makes getaddrinfo(...) return 0
//    hints.ai_family |= AI_NUMERICHOST;
  } else {
    return CLIENT_RESOLVE_ERROR;
  }
  if (getaddrinfo(remoteHost.c_str(), portString, &hints, &res) != 0) {
    errno = EFAULT;
    return CLIENT_RESOLVE_ERROR;
  }

  if ((remoteSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)
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

  return remoteSocket;
}

int ProxyListener::checkIPVersion(const std::string &address) {
/* Check for valid IPv4 or Iv6 string. Returns AF_INET for IPv4, AF_INET6 for IPv6 */

  struct in6_addr bindAddr{};
  const char *addressCString = address.c_str();
  if (inet_pton(AF_INET, addressCString, &bindAddr) == 1) {
    return AF_INET;
  } else {
    if (inet_pton(AF_INET6, addressCString, &bindAddr) == 1) {
      return AF_INET6;
    }
  }
  return -1;
}
