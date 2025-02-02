//
// Created by Rut Vora
//

// Demo main for ShapedTransceiver

#include "Server.h"
#include "Client.h"
#include <iostream>
#include <csignal>
#include <cstdarg>

// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

void addSignal(sigset_t *set, int numSignals, ...) {
  va_list args;
  va_start(args, numSignals);
  for (int i = 0; i < numSignals; i++) {
    sigaddset(set, va_arg(args, int));
  }

}

void serverOnReceive(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  (void) (stream);
  (void) (buffer);
  (void) (length);
  std::cout << "Data received..." << std::endl;
}

void RunServer() {
  QUIC::Server server("server.cert", "server.key", 4567,
                      serverOnReceive);
  server.startListening();
  std::cout << "Use ^C (Ctrl+C) to exit" << std::endl;

  // Wait for Signal to exit
  sigset_t set;
  int sig;
  int ret_val;
  sigemptyset(&set);

  addSignal(&set, 6, SIGINT, SIGKILL, SIGTERM, SIGABRT, SIGSTOP,
            SIGTSTP);
  sigprocmask(SIG_BLOCK, &set, nullptr);

  ret_val = sigwait(&set, &sig);
  if (ret_val == -1)
    perror("The signal wait failed\n");
  else {
    if (sigismember(&set, sig)) {
      std::cout << "\nExiting with signal " << sig << std::endl;
      server.stopListening();
      exit(0);
    }
  }
}

void RunClient() {
  QUIC::Client client{"localhost", 4567, [](auto &&...) {}, true};
  auto stream = client.startStream();
  std::string str = "Data...";
  auto *data = reinterpret_cast<uint8_t *>(str.data());

  // We add +1 to account for the trailing \0 in C Strings
  client.send(stream, data, str.size() + 1);

  std::string s;
  std::cin >> s;
  std::cout << s;
}

// Main

int main(int argc, char **argv) {
  std::string usage = "Usage: ./shapedTransciever <option> "
                      "\n Options: "
                      "\n\t-s : Run the client"
                      "\n\t-r : Run the server";

  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }
  if (argv[1][1] == 's') {
    RunClient();
  } else if (argv[1][1] == 'r') {
    RunServer();
  } else {
    std::cout << usage << std::endl;
  }
  return 0;
}