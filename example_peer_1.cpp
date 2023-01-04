//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include "ShapedTransciever/Sender.h"
#include "UnshapedTransciever/Receiver.h"

// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

UnshapedTransciever::Receiver *unshapedReceiver;
ShapedTransciever::Sender *shapedSender;
MsQuicStream *stream;

int clientSocket;

ssize_t receivedUnshapedData(int fromSocket, uint8_t *buffer, size_t length) {
  clientSocket = fromSocket;
  return shapedSender->send(stream, length, buffer);
}

int main() {
  // Connect to the other middlebox
  shapedSender = new ShapedTransciever::Sender{"localhost", 4567,
                                               true,
                                               ShapedTransciever::Sender::DEBUG,
                                               100000};
  stream = shapedSender->startStream();

  // Start listening for unshaped traffic
  unshapedReceiver = new UnshapedTransciever::Receiver{"", 8000,
                                                       receivedUnshapedData};
  unshapedReceiver->startListening();

  // Dummy blocking function
  std::string s;
  std::cin >> s;
}