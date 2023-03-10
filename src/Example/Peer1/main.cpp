//
// Created by Rut Vora
//

#include <sys/prctl.h>
#include "UnshapedReceiver.h"
#include "ShapedSender.h"

UnshapedReceiver *unshapedReceiver = nullptr;
ShapedSender *shapedSender = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
//  if (unshapedReceiver != nullptr && shapedSender == nullptr) {
//    unshapedReceiver->handleQueueSignal(signum);
//  } else
  if (shapedSender != nullptr && unshapedReceiver == nullptr) {
    shapedSender->handleQueueSignal(signum);
  } else if (shapedSender == nullptr && unshapedReceiver != nullptr) {
    unshapedReceiver->handleQueueSignal(signum);
  } else {
    if (unshapedReceiver != nullptr && shapedSender != nullptr)
      std::cerr << "Peer1: Both pointers present! " << getpid() << std::endl;
    else if (unshapedReceiver == nullptr && shapedSender == nullptr)
      std::cerr << "Peer1: Neither pointers present! " << getpid() << std::endl;
    else
      std::cerr << "Peer1: Huh? " << getpid() << std::endl;
    exit(1);
  }
}

int main() {
  int maxClients;
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> maxClients;

  std::string appName = "minesVPNPeer1";

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Receiver

    // This process should get a SIGHUP when it's parent (the shaped
    // sender) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    unshapedReceiver = new UnshapedReceiver{appName, maxClients};
  } else {
    // Parent Process - Shaped Sender
    sleep(2); // Wait for unshapedReceiver to initialise
    MsQuic = new MsQuicApi{};
    shapedSender = new ShapedSender{appName, maxClients};
    sleep(2);
    std::cout << "Peer is ready!" << std::endl;
  }

  // Wait for signal to exit
  waitForSignal();
}