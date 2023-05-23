//
// Created by Rut Vora
//

#ifndef MINESVPN_QUIC_CLIENT_H
#define MINESVPN_QUIC_CLIENT_H

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(X) (void)(X)
#endif

#include "msquic.hpp"
#include <string>
#include <functional>
#include "../Common.h"
#include "QUICBase.h"
#include <condition_variable>

namespace QUIC {
  class Client : public QUICBase {
  public:
    /**
     * @brief Start a new stream on this connection
     * @return The stream pointer
     */
    MsQuicStream *startStream();

    bool send(MsQuicStream *stream, uint8_t *data, size_t length) override;

    /**
     * @brief Default constructor for the client
     * @param serverName The server to connect to
     * @param port The port of the server to connect to
     * @param onReceiveFunc The function to call when a message is received
     * on a stream
     * @param noServerValidation Don't validate server certificate if true
     * @param [opt] _logLevel The log level (DEBUG, WARNING, ERROR)
     * @param [opt] _idleTimeoutMs The time after which the connection will be
     * closed
     */
    Client(const std::string &serverName, uint16_t port,
           std::function<void(MsQuicStream *stream,
                              uint8_t *buffer,
                              size_t length)> onReceiveFunc,
           bool noServerValidation = false, logLevels _logLevel = DEBUG,
           uint64_t idleTimeoutMs = 1000);


  private:
    std::mutex connectionLock;
    std::condition_variable connected;
    bool isConnected = false;

    MsQuicConfiguration *configuration;
    MsQuicConnection *connection;


    /**
     * @brief load the client configuration
     * @param noServerValidation Don't validate the server certificate
     * @return TRUE if configuration is valid and loaded successfully
     */
    bool loadConfiguration(bool noServerValidation);

    /**
     * @brief Callback handler for all QUIC connection events
     * @param connection The connection which triggered the event
     * @param context The context of the event
     * @param event The event that occurred
     * @return QUIC_STATUS (SUCCESS/FAIL)
     */
    static QUIC_STATUS connectionHandler(MsQuicConnection *connection,
                                         void *context,
                                         QUIC_CONNECTION_EVENT *event);

    /**
     * @brief Callback handler for all QUIC Events on the given stream
     * @param stream The stream on which the event occurred
     * @param context The context of the event
     * @param event The event that occurred
     * @return QUIC_STATUS (SUCCESS/FAIL)
     */
    static QUIC_STATUS streamCallbackHandler(MsQuicStream *stream,
                                             void *context,
                                             QUIC_STREAM_EVENT *event);

    void log(logLevels logLevel, const std::string &log) override;
  };
}

#endif //MINESVPN_QUIC_CLIENT_H
