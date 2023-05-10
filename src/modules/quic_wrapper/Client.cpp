//
// Created by Rut Vora
//

#include "Client.h"
#include <sstream>
#include <iostream>
#include <utility>
#include <ctime>
#include <iomanip>

extern std::vector<std::vector<uint64_t>> quicSend;

namespace QUIC {
  void Client::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "QuicClient:DEBUG: ";
        break;
      case ERROR:
        levelStr = "QuicClient:ERROR: ";
        break;
      case WARNING:
        levelStr = "QuicClient:WARNING: ";
        break;

    }
    if (logLevel >= level) {
      std::cerr << std::put_time(localTime, "[%H:%M:%S] ") << levelStr
                << log << std::endl;
    }
  }

  QUIC_STATUS Client::streamCallbackHandler(MsQuicStream *stream,
                                            void *context,
                                            QUIC_STREAM_EVENT *event) {

    auto *client = reinterpret_cast<Client *>(context);
    const void *streamPtr = static_cast<const void *>(stream);
    std::stringstream ss;
    ss << "[Stream] " << streamPtr << " ";
    switch (event->Type) {
      case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        stream->Shutdown(0);
        ss << "shut down as peer aborted";
        client->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_PEER_ACCEPTED:
#ifdef DEBUGGING
        ss << "Stream accepted by peer";
        client->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        //Send a FIN
        stream->Send(nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr);
        ss << "shut down as peer sent a shutdown signal";
        client->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_RECEIVE: {
        auto bufferCount = event->RECEIVE.BufferCount;
#ifdef DEBUGGING
        ss << "Received data from peer: ";
#endif
        for (uint32_t i = 0; i < bufferCount; i++) {
          client->onReceive(stream, event->RECEIVE.Buffers[i].Buffer,
                            event->RECEIVE.Buffers[i].Length);
#ifdef DEBUGGING
          auto length = event->RECEIVE.Buffers[i].Length;
          ss << " \n\t Length: " << length;
        }
        client->log(DEBUG, ss.str());
#else
        }
#endif
      }
        break;

      case QUIC_STREAM_EVENT_SEND_COMPLETE: {
        ctx *contextPtr =
            reinterpret_cast<ctx *>(event->SEND_COMPLETE.ClientContext);
#ifdef RECORD_STATS
        auto end = std::chrono::steady_clock::now();
        quicSend[stream->ID() / 4].push_back((end - contextPtr->start).count());
#endif
        free(contextPtr->buffer->Buffer); // The data that was sent
        free(contextPtr->buffer); // The QUIC_BUFFER struct
        free(contextPtr); // The ctx struct
      }
#ifdef DEBUGGING
        ss << "Finished a call to streamSend";
        client->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        //Automatically handled as cleanUpAutoDelete is set when creating the
        // stream class instance in connectionHandler
        ss << "The stream was shutdown and cleaned up successfully";
        client->log(WARNING, ss.str());
        break;

      case QUIC_STREAM_EVENT_START_COMPLETE:
#ifdef DEBUGGING
        ss << "started";
        client->log(DEBUG, ss.str());
#endif
        break;

      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;

  }

  QUIC_STATUS Client::connectionHandler(MsQuicConnection *connection,
                                        void *context,
                                        QUIC_CONNECTION_EVENT *event) {

    auto *client = reinterpret_cast<Client *>(context);
    MsQuicStream *stream;
    std::stringstream ss;
    const void *connectionPtr = static_cast<const void *>(connection);
    ss << "[Connection] " << connectionPtr << " ";

    switch (event->Type) {
      case QUIC_CONNECTION_EVENT_CONNECTED:
        // The handshake has completed for the connection.
#ifdef DEBUGGING
        ss << "Connected";
        client->log(DEBUG, ss.str());
#endif
        client->isConnected = true;
        client->connected.notify_all();
        break;

      case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                  CleanUpAutoDelete,
                                  streamCallbackHandler);
#ifdef DEBUGGING
        {
          const void *streamPtr = static_cast<const void *>(stream);
          ss << "Stream " << streamPtr << " started";
          client->log(DEBUG, ss.str());
        }
#endif
        break;

      case QUIC_CONNECTION_EVENT_RESUMED:
#ifdef DEBUGGING
        ss << "resumed";
        client->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
#ifdef DEBUGGING
        ss << "Received the following resumption ticket: ";
        for (uint32_t i = 0; i < event->RESUMPTION_TICKET_RECEIVED
            .ResumptionTicketLength; i++) {
          ss << event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i];
        }
        client->log(DEBUG, ss.str());
#endif
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        connection->Close();

        ss << "closed successfully";
        client->log(WARNING, ss.str());
        break;

      case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        ss << "shut down by peer";
        client->log(WARNING, ss.str());
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
          client->log(DEBUG, ss.str());
#endif
        } else {
          ss << "shut down by underlying transport layer";
          client->log(WARNING, ss.str());
        }
        break;

      default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
  }

  bool Client::loadConfiguration(bool noServerValidation) {
    auto *settings = new MsQuicSettings;
    settings->SetSendBufferingEnabled(false);
    settings->SetPacingEnabled(false);
    settings->SetKeepAlive(idleTimeoutMs / 2);
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
#ifdef DEBUGGING
      log(DEBUG, "Configuration loaded successfully!");
#endif
      return true;
    }
    log(ERROR, "Error loading configuration");
    return false;
  }

  Client::Client(const std::string &serverName, uint16_t port,
                 std::function<void(MsQuicStream *stream,
                                    uint8_t *buffer,
                                    size_t length)> onReceiveFunc,
                 bool noServerValidation,
                 logLevels _logLevel,
                 uint64_t _idleTimeoutMs) : idleTimeoutMs
                                                (_idleTimeoutMs), configuration
                                                (nullptr), connection(nullptr) {
    onReceive = std::move(onReceiveFunc);
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

  MsQuicStream *Client::startStream() {
    std::unique_lock<std::mutex> lock(connectionLock);
    connected.wait(lock, [this]() { return isConnected; });
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

  bool Client::send(MsQuicStream *stream, uint8_t *data, size_t length) {
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
    ctx *context = reinterpret_cast<ctx *>(malloc(sizeof(ctx)));
    context->client = this;
    context->buffer = SendBuffer;
#ifdef RECORD_STATS
    context->start = std::chrono::steady_clock::now();
#endif

    if (QUIC_FAILED(
        stream->Send(SendBuffer, 1, QUIC_SEND_FLAG_NONE, context))) {
      std::stringstream ss;
      ss << "[Stream " << stream->ID() << "] ";
      ss << " could not send data";
      log(ERROR, ss.str());
      free(SendBuffer);
      return false;
    }
#ifdef DEBUGGING
    std::stringstream ss;
    ss << "[Stream " << stream->ID() << "] ";
    ss << " Data sent successfully";
    log(DEBUG, ss.str());
#endif
    return true;
  }
}