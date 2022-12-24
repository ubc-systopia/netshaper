//
// Created by ubuntu on 12/23/22.
//

// Demo main for ShapedTransciever

#include "Receiver.h"
#include <iostream>

int main() {

  std::cout << "Starting Receiver" << std::endl;
  Receiver receiver("server.cert", "server.key");
  receiver.startListening();
  std::cout << "Use ^C (Ctrl+C) to exit" << std::endl;

  return 0;
}