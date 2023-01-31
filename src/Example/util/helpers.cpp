//
// Created by Rut Vora
//

#include "helpers.h"

struct SignalInfo::queueInfo SignalInfo::dequeue() {
  struct SignalInfo::queueInfo info{};
  signalQueue.pop(reinterpret_cast<uint8_t *>(&info), sizeof(info));
  return info;
}

ssize_t SignalInfo::enqueue(struct SignalInfo::queueInfo &info) {
  return signalQueue.push(reinterpret_cast<uint8_t *>(&info), sizeof(info));
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