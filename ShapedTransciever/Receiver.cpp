//
// Created by ubuntu on 12/23/22.
//

#include <stdexcept>
#include <iostream>
#include "Receiver.h"

// Load the API table. Necessary before any calls to MsQuic
const MsQuicApi * MsQuic = new MsQuicApi();

QUIC_STATUS Receiver::callback(HQUIC Listener, void *Context,
                               QUIC_LISTENER_EVENT *Event) {
  // TODO: Replace this dummy
  (void) Listener;
  (void) Context;
  (void) Event;
  return 0;
}

BOOLEAN Receiver::loadConfiguration(const std::string &certFile,
                                    const std::string &keyFile) {
  // The settings for the QUIC Connection
  auto *settings = new MsQuicSettings;
  settings->SetIdleTimeoutMs(idleTimeoutMs);
  settings->SetServerResumptionLevel(QUIC_SERVER_RESUME_AND_ZERORTT);

  // Configures the server's settings to allow for the peer to open a single
  // bidirectional stream. By default, connections are not configured to allow
  // any streams from the peer.
  //
  settings->SetPeerBidiStreamCount(1);

  // Load the X509 certificate
  QUIC_CREDENTIAL_CONFIG config;
  QUIC_CERTIFICATE_FILE certificateFileStruct;
  config.CertificateFile = &certificateFileStruct;
  config.CertificateFile->CertificateFile = certFile.c_str();
  config.CertificateFile->PrivateKeyFile = keyFile.c_str();
  config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;


  MsQuicCredentialConfig credConfig(config);

  MsQuicConfiguration configuration(reg, alpn, *settings,
                                    credConfig);
  if (configuration.IsValid()) {
    std::cout << "Configuration Loaded" << std::endl;
    return TRUE;
  }
  return FALSE;
}

Receiver::Receiver(const std::string &certFile, const std::string &keyFile,
                   int port) {

  loadConfiguration(certFile, keyFile);
  listener = nullptr;
  addr = new QuicAddr(QUIC_ADDRESS_FAMILY_INET, true);
  addr->SetPort(port);
}

void Receiver::startListening() {
  // Open the QUIC Listener (Receiver) for the given application and register
  // a callback function that is called for all events
  if(listener != nullptr) return;

  listener = new MsQuicListener(reg, this->callback);
  listener->Start(alpn, &addr->SockAddr);
}

void Receiver::stopListening() {
  delete listener;
  listener = nullptr;
}