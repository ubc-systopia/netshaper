//
// Created by Rut Vora
//

#include "helpers.h"

bool SignalInfo::dequeue(Direction direction, SignalInfo::queueInfo &info) {
  switch (direction) {
    case toShaped:
      return signalQueueToShaped.pop(reinterpret_cast<uint8_t *>(&info),
                                     sizeof(info)) >= 0;
    case fromShaped:
      return signalQueueFromShaped.pop(reinterpret_cast<uint8_t *>(&info),
                                       sizeof(info)) >= 0;
    default:
      return false;
  }
}

ssize_t SignalInfo::enqueue(Direction direction, SignalInfo::queueInfo &info) {
  switch (direction) {
    case toShaped:
      return signalQueueToShaped.push(reinterpret_cast<uint8_t *>(&info),
                                      sizeof(info));
    case fromShaped:
      return signalQueueFromShaped.push(reinterpret_cast<uint8_t *>(&info),
                                        sizeof(info));
    default:
      return false;
  }
}

void addSignal(sigset_t *set, int numSignals, ...) {
  va_list args;
  va_start(args, numSignals);
  for (int i = 0; i < numSignals; i++) {
    sigaddset(set, va_arg(args, int));
  }

}

void waitForSignal() {
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
      exit(0);
    }
  }
}