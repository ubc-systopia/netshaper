//
// Created by Rut Vora
//

#ifndef MINESVPN_QUIC_SERVER_H
#define MINESVPN_QUIC_SERVER_H

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(X) (void)(X)
#endif

#include <string>
#include <functional>
#include "msquic.hpp"
#include "../Common.h"

namespace QUIC {
  class Server {
  public:
    /**
     * @brief Default constructor
     * @param [req] certFile Path to X.509 certificate
     * @param [req] keyFile Path to private key associated with X.509 certificate
     * @param [opt] port Port to listen on (defaults to 4567)
     * @param [opt] onReceiveFunc The function to call for each received buffer.
     * Defaults to a noOp function
     * @param [opt] _logLevel The log level (DEBUG, WARNING, ERROR)
     * @param [opt] maxPeerStreams The maximum number of streams the peer is
     * allowed to start
     * @param [opt] idleTimeoutMs The time after which the connection will be
     * closed
     */
    Server(const std::string &certFile, const std::string &keyFile,
           int port = 4567, std::function<void(MsQuicStream *stream,
                                               uint8_t *buffer,
                                               size_t length)> onReceiveFunc
    = [](auto &&...) {},
           logLevels _logLevel = DEBUG, int maxPeerStreams = 1,
           uint64_t idleTimeoutMs = 1000);

    /**
     * @brief Start Listening on this server
     */
    void startListening();

    /**
     * @brief Stop Listening on this server. You can start again by
     * calling startListening() anytime on the Server object
     */
    void stopListening();

    /**
     * @brief Send data on given stream
     * @param stream The stream to send the data on
     * @param data The buffer which holds the data to be sent
     * @param length The length of the data to be sent
     * @return
     */
    bool send(MsQuicStream *stream, uint8_t *data, size_t length);

  private:
    MsQuicConfiguration *configuration;
    MsQuicAutoAcceptListener *listener;

    // Address to listen at
    QuicAddr *addr;

    // Configures the server's settings to allow for the peer to open a single
    // bidirectional stream. By default, connections are not configured to allow
    // any streams from the peer.
    //
    const int maxPeerStreams;
    const uint64_t idleTimeoutMs;

    // Configuration parameters
    inline static const QUIC_EXECUTION_PROFILE profile =
        QUIC_EXECUTION_PROFILE_LOW_LATENCY;
    inline static const bool autoCleanup = true;
    inline static const std::string appName = "minesVPN";
    const enum logLevels logLevel;

    struct ctx {
      Server *server;
      QUIC_BUFFER *buffer;
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

    /**
     * @brief load the X.509 certificate and private file
     * @param certFile The path to the X.509 certificate
     * @param keyFileThe path to the private key associated with the X.509
     * certificate
     * @return TRUE if configuration is valid and loaded successfully
     */
    bool
    loadConfiguration(const std::string &certFile, const std::string &keyFile);

    /**
     * @brief Callback handler for all QUIC Events on the address we are
     * listening at
     * @param connection The connection which triggered the event
     * @param context The Server class that opened this connection
     * @param event The event that occurred
     * @return QUIC_STATUS (SUCCESS/FAIL)
     */
    static QUIC_STATUS connectionHandler(MsQuicConnection *connection,
                                         void *context,
                                         QUIC_CONNECTION_EVENT *event);

    /**
     * @brief Callback handler for all QUIC Events on the given stream
     * @param stream The stream on which the event occurred
     * @param context The Server class that opened this connection
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
     * @param stream The stream on which data was received
     * @param buffer The byte-array that was received
     * @param length The length of the data in buffer
     */
    std::function<void(MsQuicStream *stream, uint8_t *buffer, size_t length)>
        onReceive;
  };
}

#endif //MINESVPN_QUIC_SERVER_H