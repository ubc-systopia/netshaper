//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_SENDER_H
#define MINESVPN_SHAPED_SENDER_H

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(X) (void)(X)
#endif

#include "msquic.hpp"
#include <string>

namespace ShapedTransciever {
    class Sender {
    public:
        enum logLevels {
            ERROR, WARNING, DEBUG
        };

        /**
         * @brief Start a new stream on this sender/connection
         * @return The stream pointer
         */
        MsQuicStream *startStream();

        /**
         * @brief Send data on the given stream
         * @param stream The stream to send the data on
         * @param buffer The data to be sent
         * @return
         */
        bool send(MsQuicStream *stream, size_t length, uint8_t *data);

        /**
         * @brief Default constructor for the sender
         * @param serverName The server/receiver to connect to
         * @param port The port of the server/receiver to connect to
         * @param noServerValidation Don't validate server certificate if true
         * @param [opt] _logLevel The log level (DEBUG, WARNING, ERROR)
         * @param [opt] _idleTimeoutMs The time after which the connection will be
         * closed
         */
        Sender(const std::string &serverName, uint16_t port,
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
        bool connected = false;

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
    };
}

#endif //MINESVPN_SHAPED_SENDER_H
