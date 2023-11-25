//
// Created by Rut Vora
//

#ifndef MINESVPN_QUICBASE_H
#define MINESVPN_QUICBASE_H

#include "msquic.hpp"

namespace QUIC {
  class QUICBase {
  public:

    /**
       * @brief Send data on given stream
       * @param stream The stream to send the data on
       * @param data The buffer which holds the data to be sent
       * @param length The length of the data to be sent
       * @return
       */
    virtual bool send(MsQuicStream *stream, uint8_t *data, size_t length) = 0;

  protected:
    // Configuration parameters
    inline static const QUIC_EXECUTION_PROFILE profile =
        QUIC_EXECUTION_PROFILE_LOW_LATENCY;
    inline static const bool autoCleanup = true;
    inline static const std::string appName = "minesVPN";
    enum logLevels logLevel;
    uint64_t idleTimeoutMs;
    struct ctx {
      QUIC_BUFFER *buffer = nullptr;
    };

    MsQuicRegistration *reg;

    // MsQuicAlpn is the "Application Layer Protocol Negotiation" string.
    // Specifies which Application Protocol layers are available. See RFC 7301
    // We set this to 'sample' to be compatible with the sample.c in
    // msquic/src/tools/sample. But it could be any string.
    // For IANA validated strings: https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#alpn-protocol-ids
    const MsQuicAlpn alpn{"sample"};


    /**
     * @brief If log level set by user is equal or more verbose than the log
     * level passed to this function, print the given string
     * @param logLevel The log level of the given string
     * @param log The string to be logged
     */
    virtual void log(logLevels logLevel, const std::string &log) = 0;

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
#endif //MINESVPN_BASE_H
