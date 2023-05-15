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
#include <condition_variable>

namespace QUIC {
  class Client {
  public:
    /**
     * @brief Start a new stream on this connection
     * @return The stream pointer
     */
    MsQuicStream *startStream();

    /**
     * @brief Send data on the given stream
     * @param stream The stream to send the data on
     * @param data The data to be sent
     * @param length The length of the data to be sent
     * @return
     */
    bool send(MsQuicStream *stream, uint8_t *data, size_t length);

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
           uint64_t _idleTimeoutMs = 1000);


  private:
    // Configuration parameters
    inline static const QUIC_EXECUTION_PROFILE profile =
        QUIC_EXECUTION_PROFILE_LOW_LATENCY;
    inline static const bool autoCleanup = true;
    inline static const std::string appName = "minesVPN";
    enum logLevels logLevel;
    const uint64_t idleTimeoutMs;
    std::mutex connectionLock;
    std::condition_variable connected;
    bool isConnected = false;

    struct ctx {
      QUIC_BUFFER *buffer = nullptr;
#ifdef RECORD_STATS
      std::chrono::time_point<std::chrono::steady_clock> start;
#endif
    };
    // MsQuic is a shared library. Hence, register this application with it.
    // The name has to be unique per application on a single machine
    const MsQuicRegistration reg{appName.c_str(), profile,
                                 autoCleanup};

    // MsQuicAlpn is the "Application Layer Protocol Negotiation" string.
    // Specifies which Application Protocol layers are available. See RFC 7301
    // We set this to 'sample' to be compatible with the sample.c in
    // msquic/src/tools/sample. But it could be any string.
    // For IANA validated strings: https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#alpn-protocol-ids
    const MsQuicAlpn alpn{"sample"};
    MsQuicConfiguration *configuration;
    MsQuicConnection *connection;


    /**
     * @brief load the client configuration
     * @param noServerValidation Don't validate the server certificate
     * @return TRUE if configuration is valid and loaded successfully
     */
    bool
    loadConfiguration(bool noServerValidation);

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

    /**
     * @brief If log level set by user is equal or more verbose than the log
     * level passed to this function, print the given string
     * @param logLevel The log level of the given string
     * @param log The string to be logged
     */
    void log(logLevels logLevel, const std::string &log);

    /**
     * @brief The function that is called on each buffer that is received
     * @param stream The stream on which the data was received
     * @param buffer The byte-array that was received
     * @param length The length of the data in buffer
     */
    std::function<void(MsQuicStream *stream, uint8_t *buffer, size_t length)>
        onReceive;
  };
}

#endif //MINESVPN_QUIC_CLIENT_H
