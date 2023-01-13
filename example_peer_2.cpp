//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include "ShapedTransciever/Receiver.h"
#include "UnshapedTransciever/Sender.h"

// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

UnshapedTransciever::Sender *unshapedSender;
ShapedTransciever::Receiver *shapedReceiver;
MsQuicStream *stream;


ssize_t receivedShapedData(uint8_t *buffer, size_t length) {
  return unshapedSender->sendData(buffer, length);
}

int main() {
  std::string remoteHost;
  int remotePort;

  // Start listening (unshaped data)
  std::cout << "Enter the IP address of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remoteHost;

  std::cout << "Enter the port of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remotePort;

  // Connect to remote Host and port to forward data
  unshapedSender = new UnshapedTransciever::Sender{remoteHost, remotePort};

  // Start listening for connections from the other middlebox
  shapedReceiver =
      new ShapedTransciever::Receiver{"server.cert", "server.key",
                                      4567,
                                      receivedShapedData,
                                      ShapedTransciever::Receiver::WARNING,
                                      1,
                                      100000};
  shapedReceiver->startListening();

  // Dummy blocking function
  std::string s;
  std::cin >> s;

}