//
// Created by Rut Vora
//

#include "Sender.h"
#include <sstream>
#include <iostream>
#include <utility>
#include <ctime>
#include <iomanip>

namespace QUIC {
  void Sender::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "QS:DEBUG: ";
        break;
      case ERROR:
        levelStr = "QS:ERROR: ";
        break;
      case WARNING:
        levelStr = "QS:WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }

  QUIC_STATUS Sender::streamCallbackHandler(MsQuicStream *stream,
                                            void *context,
                                            QUIC_STREAM_EVENT *event) {

    auto *sender = (Sender *) context;
    const void *streamPtr = static_cast<const void *>(stream);
    std::stringstream ss;
    ss << "[Stream] " << streamPtr << " ";
    switch (event->Type) {
      case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        stream->Shutdown(0);
        ss << "shut down as peer aborted";
        sender->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_PEER_ACCEPTED:
        ss << "Stream accepted by peer";
        sender->log(DEBUG, ss.str());
        break;

      case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        // TODO Finish Sending
        //Send a FIN
        stream->Send(nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr);
        ss << "shut down as peer sent a shutdown signal";
        sender->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_RECEIVE: {
        auto bufferCount = event->RECEIVE.BufferCount;
        ss << "Received data from peer: ";
        for (uint32_t i = 0; i < bufferCount; i++) {
          sender->onResponse(stream, event->RECEIVE.Buffers[i].Buffer,
                             event->RECEIVE.Buffers[i].Length);

          auto length = event->RECEIVE.Buffers[i].Length;
          ss << " \n\t Length: " << length;
        }
        sender->log(DEBUG, ss.str());
      }
        break;

      case QUIC_STREAM_EVENT_SEND_COMPLETE:
//      free(event->SEND_COMPLETE.ClientContext);
        ss << "Finished a call to streamSend";
        sender->log(DEBUG, ss.str());
        break;

      case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        //Automatically handled as cleanUpAutoDelete is set when creating the
        // stream class instance in connectionHandler
        ss << "The stream was shutdown and cleaned up successfully";
        sender->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_START_COMPLETE:
        ss << "started";
        sender->log(DEBUG, ss.str());
        break;

      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;

  }

  QUIC_STATUS Sender::connectionHandler(MsQuicConnection *connection,
                                        void *context,
                                        QUIC_CONNECTION_EVENT *event) {

    auto *sender = (Sender *) context;
    MsQuicStream *stream;
    std::stringstream ss;
    const void *connectionPtr = static_cast<const void *>(connection);
    ss << "[Connection] " << connectionPtr << " ";

    switch (event->Type) {
      case QUIC_CONNECTION_EVENT_CONNECTED:
        // The handshake has completed for the connection.
        ss << "Connected";
        sender->log(DEBUG, ss.str());
        sender->connected = true;
        break;

      case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                  CleanUpAutoDelete,
                                  streamCallbackHandler);
        {
          const void *streamPtr = static_cast<const void *>(stream);

          ss << "Stream " << streamPtr << " started";
          sender->log(DEBUG, ss.str());
        }
        break;

      case QUIC_CONNECTION_EVENT_RESUMED:
        ss << "resumed";
        sender->log(DEBUG, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
        ss << "Received the following resumption ticket: ";
        for (uint32_t i = 0; i < event->RESUMPTION_TICKET_RECEIVED
            .ResumptionTicketLength; i++) {
          ss << event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i];
        }
        sender->log(DEBUG, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        connection->Close();

        ss << "closed successfully";
        sender->log(WARNING, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        ss << "shut down by peer";
        sender->log(WARNING, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        //
        // The connection has been shut down by the transport. Generally, this
        // is the expected way for the connection to shut down with this
        // protocol, since we let idle timeout kill the connection.
        //
        if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status ==
            QUIC_STATUS_CONNECTION_IDLE) {

          ss << "shutting down on idle";
          sender->log(DEBUG, ss.str());


        } else {
          ss << "shut down by underlying transport layer";
          sender->log(WARNING, ss.str());
        }
        break;

      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
  }

  bool Sender::loadConfiguration(bool noServerValidation) {
    auto *settings = new MsQuicSettings;
    settings->SetIdleTimeoutMs(idleTimeoutMs);

    // Configure default client configuration
    QUIC_CREDENTIAL_CONFIG config{};
    config.Type = QUIC_CREDENTIAL_TYPE_NONE;
    config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    if (noServerValidation) {
      config.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    }

    MsQuicCredentialConfig credConfig(config);

    configuration = new MsQuicConfiguration(reg, alpn, *settings,
                                            credConfig);
    delete settings;
    if (configuration->IsValid()) {
      log(DEBUG, "Configuration loaded successfully!");
      return true;
    }
    log(ERROR, "Error loading configuration");
    return false;
  }

  Sender::Sender(const std::string &serverName, uint16_t port,
                 std::function<void(MsQuicStream *stream,
                                    uint8_t *buffer,
                                    size_t length)> onResponseFunc,
                 bool noServerValidation,
                 logLevels _logLevel,
                 uint64_t _idleTimeoutMs) : idleTimeoutMs
                                                (_idleTimeoutMs), configuration
                                                (nullptr), connection(nullptr) {
    onResponse = std::move(onResponseFunc);
    logLevel = _logLevel;
    loadConfiguration(noServerValidation);
    connection = new MsQuicConnection(reg, autoCleanup ? CleanUpAutoDelete :
                                           CleanUpManual,
                                      connectionHandler, this);
    connection->Start(*configuration, serverName.c_str(), port);
    if (!connection->IsValid()) {
      std::stringstream ss;
      ss << "Connection to " << serverName << ":" << port << " failed!";
      log(ERROR, ss.str());
      connection->Close();
      throw std::runtime_error("Connection Failed!");
    }

  }

  MsQuicStream *Sender::startStream() {
    while (!connected);  // TODO: Improve this with wait and conditional
    MsQuicStream *stream;
    stream = new MsQuicStream{*connection, QUIC_STREAM_OPEN_FLAG_NONE,
                              autoCleanup ? CleanUpAutoDelete : CleanUpManual,
                              streamCallbackHandler, this
    };
    // variables
    if (QUIC_FAILED(stream->Start())) {
      log(ERROR, "Stream could not be started");
      throw std::runtime_error("Stream could not be started");
    }
    return stream;
  }

  bool Sender::send(MsQuicStream *stream, uint8_t *data, size_t length) {
    // Note: Valgrind reports this as a "definitely lost" block. But QUIC
    //  frees it from another thread after sending is complete
    auto SendBuffer =
        reinterpret_cast<QUIC_BUFFER *>(malloc(sizeof(QUIC_BUFFER)));
    if (SendBuffer == nullptr) {
      log(ERROR, "Memory allocation for the send buffer failed");
      return false;
    }

    SendBuffer->Buffer = data;
    SendBuffer->Length = length;

    uint64_t streamID;
    stream->GetID(&streamID);
    std::stringstream ss;
    ss << "[Stream " << streamID << "] ";
    ss << " sentLength=" << length;
    log(DEBUG, ss.str());

    if (QUIC_FAILED(stream->Send(SendBuffer, 1, QUIC_SEND_FLAG_NONE, this))) {
      ss << " could not send data";
      log(ERROR, ss.str());
      free(SendBuffer);
      return false;
    }

    ss << " Data sent successfully";
    log(DEBUG, ss.str());
    return true;
  }
}