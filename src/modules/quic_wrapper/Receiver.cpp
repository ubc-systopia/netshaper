//
// Created by Rut Vora
//

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <utility>
#include <ctime>
#include <iomanip>
#include "Receiver.h"

namespace QUIC {
  void Receiver::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "QR:DEBUG: ";
        break;
      case ERROR:
        levelStr = "QR:ERROR: ";
        break;
      case WARNING:
        levelStr = "QR:WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }

  QUIC_STATUS Receiver::streamCallbackHandler(MsQuicStream *stream,
                                              void *context,
                                              QUIC_STREAM_EVENT *event) {
    auto *receiver = (Receiver *) context;

    const void *streamPtr = static_cast<const void *>(stream);
    std::stringstream ss;
    ss << "[Stream] " << streamPtr << " ";
    switch (event->Type) {
      case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        stream->Shutdown(0);
        ss << "shut down as peer aborted";
        receiver->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        // TODO Finish Sending

        //Send a FIN
        stream->Send(nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr);
        ss << "shut down as peer sent a shutdown signal";
        receiver->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_RECEIVE: {
        auto bufferCount = event->RECEIVE.BufferCount;
#ifdef DEBUGGING
        ss << "Received data from peer: ";
#endif
        for (uint32_t i = 0; i < bufferCount; i++) {
          receiver->onReceive(stream, event->RECEIVE.Buffers[i].Buffer,
                              event->RECEIVE.Buffers[i].Length);
#ifdef DEBUGGING
          auto length = event->RECEIVE.Buffers[i].Length;
          ss << " \n\t Length: " << length;
        }
        receiver->log(DEBUG, ss.str());
#else
        }
#endif
      }
        break;

      case QUIC_STREAM_EVENT_SEND_COMPLETE:
#ifdef DEBUGGING
        ss << "Finished a call to streamSend";
        receiver->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        //Automatically handled as cleanUpAutoDelete is set when creating the
        // stream class instance in connectionHandler
        ss << "The stream was shutdown and cleaned up successfully";
        receiver->log(WARNING, ss.str());
      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;

  }

  QUIC_STATUS Receiver::connectionHandler(MsQuicConnection *connection,
                                          void *context,
                                          QUIC_CONNECTION_EVENT *event) {
    auto *receiver = (Receiver *) context;

    MsQuicStream *stream;
    std::stringstream ss;
    const void *connectionPtr = static_cast<const void *>(connection);
    ss << "[Connection] " << connectionPtr << " ";

    switch (event->Type) {
      case QUIC_CONNECTION_EVENT_CONNECTED:
        // The handshake has completed for the connection.
#ifdef DEBUGGING
        ss << "Connected";
        receiver->log(DEBUG, ss.str());
#endif
        connection->SendResumptionTicket();
        break;

      case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                  CleanUpAutoDelete,
                                  streamCallbackHandler, context);
#ifdef DEBUGGING
        {
          const void *streamPtr = static_cast<const void *>(stream);
          ss << "Stream " << streamPtr << " started";
          receiver->log(DEBUG, ss.str());
        }
#endif
        break;

      case QUIC_CONNECTION_EVENT_RESUMED:
#ifdef DEBUGGING
        ss << "resumed";
        receiver->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        connection->Close();
        ss << "closed successfully";
        receiver->log(WARNING, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        ss << "shut down by peer";
        receiver->log(WARNING, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        //
        // The connection has been shut down by the transport. Generally, this
        // is the expected way for the connection to shut down with this
        // protocol, since we let idle timeout kill the connection.
        //
        if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status ==
            QUIC_STATUS_CONNECTION_IDLE) {
#ifdef DEBUGGING
          ss << "shutting down on idle";
          receiver->log(DEBUG, ss.str());
#endif
        } else {
          ss << "shut down by underlying transport layer";
          receiver->log(WARNING, ss.str());
        }
        break;

      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
  }

  bool Receiver::loadConfiguration(const std::string &certFile,
                                   const std::string &keyFile) {
    // The settings for the QUIC Connection
    auto *settings = new MsQuicSettings;
    settings->SetIdleTimeoutMs(idleTimeoutMs);
    settings->SetServerResumptionLevel(QUIC_SERVER_RESUME_AND_ZERORTT);


    settings->SetPeerBidiStreamCount(maxPeerStreams);

    // Load the X509 certificate
    QUIC_CREDENTIAL_CONFIG config{};
    QUIC_CERTIFICATE_FILE certificateFileStruct;
    config.CertificateFile = &certificateFileStruct;
    config.CertificateFile->CertificateFile = certFile.c_str();
    config.CertificateFile->PrivateKeyFile = keyFile.c_str();
    config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;


    MsQuicCredentialConfig credConfig(config);

    configuration = new MsQuicConfiguration(reg, alpn, *settings,
                                            credConfig);
    delete settings;
    if (configuration->IsValid()) {
#ifdef DEBUGGING
      log(DEBUG, "Configuration loaded successfully!");
#endif
      return true;
    }
    log(ERROR, "Error loading configuration. Are you sure the correct server "
               "certificate and keyfile are provided?");
    return false;
  }

  Receiver::Receiver(const std::string &certFile, const std::string &keyFile,
                     int port, std::function<void(MsQuicStream *stream,
                                                  uint8_t *buffer,
                                                  size_t length)> onReceiveFunc,
                     logLevels level, int maxPeerStreams, uint64_t
                     idleTimeoutMs) : configuration(nullptr),
                                      listener(nullptr),
                                      addr(new QuicAddr(
                                          QUIC_ADDRESS_FAMILY_UNSPEC)),
                                      maxPeerStreams(maxPeerStreams),
                                      idleTimeoutMs(idleTimeoutMs),
                                      logLevel(level) {
    onReceive = std::move(onReceiveFunc);
#ifdef DEBUGGING
    log(DEBUG, "Loading Configuration...");
#endif
    bool success = loadConfiguration(certFile, keyFile);
    if (!success) {
      exit(1);
    }
    addr->SetPort(port);

    {
      std::stringstream ss;
      ss << (alpn.operator const QUIC_BUFFER *())[0].Buffer;
#ifdef DEBUGGING
      log(DEBUG, "ALPN: " + ss.str());
    }
    log(DEBUG, "Port: " + std::to_string(port));
#else
    }
#endif
  }

  void Receiver::startListening() {
    // Open the QUIC Listener (Receiver) for the given application and register
    // a listenerCallback function that is called for all events
    if (listener != nullptr) {
      log(WARNING, "startListening called on a receiver that's already "
                   "listening");
      return;
    }

    listener = new MsQuicAutoAcceptListener(reg, *configuration,
                                            this->connectionHandler, this);
    listener->Start(alpn, &addr->SockAddr);
#ifdef DEBUGGING
    log(DEBUG, "Started listening");
#endif
  }

  void Receiver::stopListening() {
    delete listener;
    listener = nullptr;
#ifdef DEBUGGING
    log(DEBUG, "Stopped listening");
#endif
  }
}