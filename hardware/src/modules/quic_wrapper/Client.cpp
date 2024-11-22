//
// Created by Rut Vora
//

#include "Client.h"

#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sstream>
#include <iostream>
#include <utility>
#include <ctime>
#include <iomanip>

namespace QUIC {
  void Client::log(logLevels level, const std::string &log) {
    auto time = std::time(nullptr);
    auto localTime = std::localtime(&time);
    std::string levelStr;
    switch (level) {
      case DEBUG:
        levelStr = "QuicClient:DEBUG: ";
        break;
      case INFO:
        levelStr = "QuicClient:INFO: ";
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

    configuration = new MsQuicConfiguration(*reg, alpn, *settings,
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
                 uint64_t idleTimeoutMs) : configuration(nullptr),
                                           connection(nullptr) {
    memset(&tx_timestamping_addr, 0, sizeof(tx_timestamping_addr));
    tx_timestamping_addr.sin_family = AF_INET;
    tx_timestamping_addr.sin_port = htons(1234);
    if (inet_pton(AF_INET, "192.168.6.2", &tx_timestamping_addr.sin_addr) <= 0) {
      log(ERROR, "Invalid address/ Address not supported");
      throw std::runtime_error("Invalid address/ Address not supported");
    }

    tx_timestamping_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tx_timestamping_fd < 0) {
      log(ERROR, "Socket creation failed");
      throw std::runtime_error("Socket creation failed");
    }

    struct ifreq ifr;
    struct hwtstamp_config cfg;
    memset(&ifr, 0, sizeof(ifr));
    memset(&cfg, 0, sizeof(cfg));
    strncpy(ifr.ifr_name, "enp1s0f0np0", sizeof(ifr.ifr_name));

    cfg.tx_type = HWTSTAMP_TX_ON;
    cfg.rx_filter = HWTSTAMP_FILTER_ALL;

    ifr.ifr_data = (char *)&cfg;

    if (ioctl(tx_timestamping_fd, SIOCSHWTSTAMP, &ifr) < 0) {
     log(ERROR, "Could not set hardware timestamping");
     throw std::runtime_error("Could not set hardware timestamping");
    }

    uint32_t timestampingVal = SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE
        | SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_RAW_HARDWARE |
        SOF_TIMESTAMPING_SOFTWARE;
    if (setsockopt(tx_timestamping_fd, SOL_SOCKET, SO_TIMESTAMPING, &timestampingVal,
                     sizeof(timestampingVal)) < 0) {
        log(ERROR, "Setting socket options failed");
        throw std::runtime_error("Setting socket options failed");
    }

    reg = new MsQuicRegistration{appName.c_str(), profile, autoCleanup};
    this->idleTimeoutMs = idleTimeoutMs;
    onReceive = std::move(onReceiveFunc);
    logLevel = _logLevel;
    loadConfiguration(noServerValidation);
    connection = new MsQuicConnection(*reg, autoCleanup ? CleanUpAutoDelete :
                                            CleanUpManual,
                                      connectionHandler, this);
#ifdef DEBUGGING
    std::stringstream ss;
    ss << "Connecting to " << serverName << ":" << port;
    log(DEBUG, ss.str());
#endif
    connection->Start(*configuration, serverName.c_str(), port);
    if (!connection->IsValid()) {
      std::stringstream ss;
      ss << "Connection to " << serverName << ":" << port << " failed!";
     log(ERROR, ss.str());
      connection->Close();
      throw std::runtime_error("Connection Failed!");
    }

#ifdef DEBUGGING
    log(DEBUG, "QUIC::Client constructor completed");
#endif
  }

  MsQuicStream *Client::startStream() {
#ifdef DEBUGGING
    log(DEBUG, "Waiting for connection to be established");
#endif
    std::unique_lock<std::mutex> lock(connectionLock);
    connected.wait(lock, [this]() { return isConnected; });
#ifdef DEBUGGING
    log(DEBUG, "Connection established");
#endif
    auto *stream = new MsQuicStream{*connection, QUIC_STREAM_OPEN_FLAG_NONE,
                                    autoCleanup ? CleanUpAutoDelete
                                                : CleanUpManual,
                                    streamCallbackHandler, this};
    if (stream->Handle == nullptr) {
      free(stream);
      stream = nullptr;
    } else {
      stream->ID(); // Fetch the ID now, so we don't fetch it later
    }
    // variables
    if (QUIC_FAILED(stream->Start())) {
      log(ERROR, "Stream could not be started");
      throw std::runtime_error("Stream could not be started");
    }
    return stream;
  }

  bool Client::send(MsQuicStream *stream, uint8_t *data, size_t length) {
    auto SendBuffer =
        reinterpret_cast<QUIC_BUFFER *>(malloc(sizeof(QUIC_BUFFER)));
    if (SendBuffer == nullptr) {
      log(ERROR, "Memory allocation for the send buffer failed");
      return false;
    }

    SendBuffer->Buffer = data;
    SendBuffer->Length = length;
    ctx *context = reinterpret_cast<ctx *>(malloc(sizeof(ctx)));
    context->buffer = SendBuffer;
    log(DEBUG, "Sending data to QUIC");
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
    ss << std::to_string(length) << "bytes sent successfully";
    log(DEBUG, ss.str());
#endif
    return true;
  }

  void Client::printCopyStats() {}

  void Client::handleScmTimestamping(const struct scm_timestamping *ts) {}
}
