//
// Created by Rut Vora
//

#ifndef PROXY_CPP_PROXYLISTENER_H
#define PROXY_CPP_PROXYLISTENER_H

#define BUF_SIZE 16384
#define BACKLOG 20 // Number of pending connections the queue should hold
#define SERVER_SOCKET_ERROR (-1)
#define SERVER_SETSOCKOPT_ERROR (-2)
#define SERVER_BIND_ERROR (-3)
#define SERVER_LISTEN_ERROR (-4)
#define CLIENT_SOCKET_ERROR (-5)
#define CLIENT_RESOLVE_ERROR (-6)
#define CLIENT_CONNECT_ERROR (-7)
//#define CREATE_PIPE_ERROR -8
//#define BROKEN_PIPE_ERROR (-9)
//#define SYNTAX_ERROR -10

#include <iostream>
#include <unistd.h>

class ProxyListener {
private:
  std::string bindAddr, remoteHost;
  int localPort, remotePort;
  int localSocket; //, clientSocket, remoteSocket;
  int inetFamily = 0; //Valid values are AF_INET or AF_INET6


  /** Opens a socket listening on bindAddr:localPort
   *
   * @return valid localSocket that the proxy is listening on
   */
  int openSocket();

  /**
   * @brief check if the given IP address is IPv4 or IPv6
   * @return returns the value AF_INET or AF_INET6 depending on the input
   * IP address
   */
  static int checkIPVersion(const std::string &);

  /**
   * @brief Handles the given client (called from serverLoop)
   * @param clientSocket The socket where the client connected
   */
  void handleClient(int clientSocket);

  /**
   * @brief Main loop of the server. Spawns of child processes for each new
   * client
   */
  void serverLoop();

  /**
   * @brief Forwards data between the given sockets. Uses a user-space buffer
   * @param fromSocket The socket to copy the data from
   * @param toSocket The socket to copy the data to
   */
  static void forwardData(int fromSocket, int toSocket);

  /**
   * @brief Open a socket connection to the remote for each client that connects
   * @return File Descriptor of opened socket to remote host
   */
  int connectToRemote();

public:

  /**
   * @brief Default constructor for the ProxyListener
   * @param remoteHost The remote host to proxy to
   * @param remotePort The port on the remote host to proxy to
   * @param bindAddr [opt] The address to listen to for proxying (defaults to "0.0.0.0")
   * @param localPort [opt] The port to listen to for proxying (defaults to 8000)
   */
  ProxyListener(std::string remoteHost, int remotePort,
                std::string bindAddr = "",
                int localPort = 8000);

  /**
   * @brief Start listening on given bind Address and port
   * Will throw an error if startup fails. Else will go into a loop
   */
  void startListening();

  ~ProxyListener();
};

#endif //PROXY_CPP_PROXYLISTENER_H
