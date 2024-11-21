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
#include "QUICBase.h"

namespace QUIC {
  class Server : public QUICBase {
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


    bool send(MsQuicStream *stream, uint8_t *data, size_t length) override;

    void printStats();

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

    void log(logLevels logLevel, const std::string &log) override;
  };
}

#endif //MINESVPN_QUIC_SERVER_H
