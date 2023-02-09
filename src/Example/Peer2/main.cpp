//
// Created by Rut Vora
//

#include "UnshapedSender.h"
#include "ShapedReceiver.h"
#include <sys/prctl.h>

UnshapedSender *unshapedSender = nullptr;
ShapedReceiver *shapedReceiver = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
  if (unshapedSender != nullptr && shapedReceiver == nullptr) {
    unshapedSender->handleQueueSignal(signum);
  } else {
    std::cerr << "Peer2: Issue with handling queue signal!" << std::endl;
    exit(1);
  }
}

int main() {
  int maxStreamsPerPeer;
  std::cout << "Enter the maximum number of streams per peer that should be "
               "supported:"
               " " << std::endl;
  std::cin >> maxStreamsPerPeer;

  std::string appName = "minesVPNPeer2";

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Sender

    // This process should get a SIGHUP when it's parent (the shaped
    // receiver) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    unshapedSender = new UnshapedSender{appName, 1, maxStreamsPerPeer};
  } else {
    // Parent Process - Shaped Receiver
    sleep(2); // Wait for unshapedSender to initialise
    MsQuic = new MsQuicApi{};
    shapedReceiver = new ShapedReceiver{appName, "server.cert", "server.key",
                                        1, maxStreamsPerPeer};
    sleep(1);
    std::cout << "Peer is ready!" << std::endl;
  }

  // Wait for signal to exit
  waitForSignal();
}