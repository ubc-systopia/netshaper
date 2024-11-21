//
// Created by Rut Vora
//

#ifndef MINESVPN_TCP_SERVER_H
#define MINESVPN_TCP_SERVER_H

#define BUF_SIZE 16384
#define BACKLOG 20 // Number of pending connections the queue should hold
#define SERVER_SOCKET_ERROR (-1)
#define SERVER_SETSOCKOPT_ERROR (-2)
#define SERVER_BIND_ERROR (-3)
#define SERVER_LISTEN_ERROR (-4)
#define CLIENT_RESOLVE_ERROR (-6)

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(X) (void)(X)
#endif


#include <iostream>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>
#include <unistd.h>
#include <sys/socket.h>
#include <functional>
#include "../Common.h"

namespace TCP {
  class Server {
  public:
    /**
 * @brief Default constructor for the ProxyListener
 * @param bindAddr [opt] The address to listen to for proxying (defaults to "0.0.0.0")
 * @param localPort [opt] The port to listen to for proxying (defaults to 8000)
 * @param [opt] onReceiveFunc The function to be called when a buffer is
 * received. Pass a free function as a function pointer, a class member
 * function by using std::bind or lambda functions
 * @param [opt] level Log Level (ERROR, WARNING, DEBUG)
 */
    explicit Server(std::string bindAddr = "",
                    int localPort = 8000,
                    std::function<bool(int fromSocket,
                                       std::string &clientAddress,
                                       uint8_t *buffer,
                                       size_t length,
                                       enum connectionStatus connStatus)>
                    onReceiveFunc = [](auto &&...) { return true; },
                    logLevels level = DEBUG);

    /**
     * @brief Start listening on given bind Address and port
     * Will throw an error if startup fails. Else will go into a loop
     */
    void startListening();

    /**
     * @brief Wrapper for send()
     * @param toSocket Send data to given socket
     * @param buffer The buffer to be sent
     * @param length The length of the buffer to be sent
     * @return
     */
    ssize_t sendData(int toSocket, uint8_t *buffer, size_t length);

    ~Server();

    static inline void sendFIN(int socket) {
      shutdown(socket, SHUT_WR);
    };

    void printStats();


  private:
    std::string bindAddr;
    int localPort;
    int localSocket;
    int inetFamily = 0; //Valid values are AF_INET or AF_INET6
    const enum logLevels logLevel;

    // Timestamps
    int timestampIndex = 0;
    struct timespec timestamps[BUF_SIZE];

    /**
     * @brief If log level set by user is equal or more verbose than the log
     * level passed to this function, print the given string
     * @param level The log level of the given string
     * @param log The string to be logged
     */
    void log(logLevels level, const std::string &log);

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
    int checkIPVersion(const std::string &);

    /**
     * @brief Handles the given client (called from serverLoop)
     * @param clientSocket The socket where the client connected
     * @param clientAddress The address of the client
     */
    void handleClient(int clientSocket, std::string clientAddress);

    /**
     * @brief Main loop of the server. Spawns of child processes for each new
     * client
     */
    [[noreturn]] void serverLoop();

    /**
     * @brief Receive data from the given socket and call serverOnReceive
     * @param socket The socket to read the data from
     */
    void receiveData(int socket, std::string &clientAddress);

    /**
     * @brief Returns a string of the form "address:port"
     * @param sockAddr The socket address structure
     * @return A string of the form "address:port"
     */
    static std::string getAddress(struct sockaddr &sockAddr);

    /**
     * @brief The function that is called on each buffer that is received
     * @param fromSocket The socket from which the buffer was received
     * @param clientAddress The address of the client
     * @param buffer The byte-array that was received
     * @param length The length of the data in buffer
     */
    std::function<bool(int fromSocket, std::string &clientAddress,
                       uint8_t *buffer, size_t length,
                       enum connectionStatus connStatus)> onReceive;

    void handleTimestamps(msghdr *msg);

    void handleScmTimestamping(const struct scm_timestamping *ts);
  };
}

#endif //MINESVPN_TCP_SERVER_H
