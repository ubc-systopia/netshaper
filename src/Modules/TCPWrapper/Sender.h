//
// Created by Rut Vora
//

#ifndef MINESVPN_TCP_SENDER_H
#define MINESVPN_TCP_SENDER_H

#define BUF_SIZE 16384
#define BACKLOG 20 // Number of pending connections the queue should hold
#define CLIENT_SOCKET_ERROR (-5)
#define CLIENT_RESOLVE_ERROR (-6)
#define CLIENT_CONNECT_ERROR (-7)

#include <iostream>
#include <unistd.h>
#include <functional>
#include <sys/socket.h>
#include "../Common.h"

namespace TCP {
  class Sender {
  public:
    /**
     * @brief Default constructor for the ProxyListener
     * @param remoteHost The remote host to proxy to
     * @param remotePort The port on the remote host to proxy to
     * @param [opt] onReceiveFunc The function to be called when a buffer is
     * received. Pass a free function as a function pointer, a class member
     * function by using std::bind or lambda functions
     * @param [opt] level Log Level (ERROR, WARNING, DEBUG)
     */
    Sender(std::string remoteHost, int remotePort,
           std::function<void(TCP::Sender *, uint8_t *buffer,
                              size_t length, connectionStatus connStatus)>
           onReceiveFunc = [](
               auto &&...) {}, logLevels level = DEBUG);

    /**
     * @brief Calls the send() function on the remoteSocket with given buffer
     * @param buffer The buffer to be sent
     * @param length The length of the data in the buffer
     * @return The number of bytes sent
     */
    ssize_t sendData(uint8_t *buffer, size_t length);

    ~Sender();

    inline void sendFIN() const {
      shutdown(remoteSocket, SHUT_WR);
    }


  private:
    std::string remoteHost;
    int remotePort;
    const enum logLevels logLevel;
    int remoteSocket;

    /**
     * @brief If log level set by user is equal or more verbose than the log
     * level passed to this function, print the given string
     * @param level The log level of the given string
     * @param log The string to be logged
     */
    void log(logLevels level, const std::string &log);

    /**
     * @brief check if the given IP address is IPv4 or IPv6
     * @return returns the value AF_INET or AF_INET6 depending on the input
     * IP address
     */
    int checkIPVersion(const std::string &);

    /**
     * @brief Open a socket connection to the remote for each client that connects
     * @return File Descriptor of opened socket to remote host
     */
    int connectToRemote();


    void startReceiving();

    /**
     * @brief The function that is called on each buffer that is received
     * @param buffer The byte-array that was received
     * @param length The length of the data in buffer
     */
    std::function<void(TCP::Sender *, uint8_t *buffer,
                       size_t length, connectionStatus connStatus)> onReceive;
  };
}

#endif //MINESVPN_TCP_SENDER_H