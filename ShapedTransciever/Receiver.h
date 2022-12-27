//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_RECEIVER_H
#define MINESVPN_SHAPED_RECEIVER_H

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(X) (void)(X)
#endif

#include<string>
#include "msquic.hpp"

class Receiver {
private:
  // Configuration parameters
  inline static const QUIC_EXECUTION_PROFILE profile =
      QUIC_EXECUTION_PROFILE_LOW_LATENCY;
  inline static const bool autoCleanup = true;
  inline static const std::string appName = "minesVPN";
  enum logLevels {
    ERROR, WARNING, DEBUG
  };
  inline static enum logLevels logLevel;


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
  MsQuicAutoAcceptListener *listener;

  // Address to listen at
  QuicAddr *addr;

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
   * @param Listener The listener which triggered the event (in case of
   * multiple listeners)
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
  static void log(logLevels logLevel, const std::string &log);

public:
  // Configures the server's settings to allow for the peer to open a single
  // bidirectional stream. By default, connections are not configured to allow
  // any streams from the peer.
  //
  const int maxPeerStreams;
  const uint64_t idleTimeoutMs;

  /**
   * @brief Default constructor
   * @param [req] certFile Path to X.509 certificate
   * @param [req] keyFile Path to private key associated with X.509 certificate
   * @param [opt] port Port to listen on (defaults to 4567)
   */
  Receiver(const std::string &certFile, const std::string &keyFile,
           int port = 4567, logLevels _logLevel = DEBUG,
           int _maxPeerStreams = 1, uint64_t _idleTimeoutMs = 1000);

  /**
   * @brief Start Listening on this receiver
   */
  void startListening();

  /**
   * @brief Stop Listening on this receiver. You can start again by calling
   * startListening() anytime on the Receiver object
   */
  void stopListening();
};


#endif //MINESVPN_SHAPED_RECEIVER_H
