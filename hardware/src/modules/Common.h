//
// Created by Rut Vora
//

#ifndef MINESVPN_COMMON_H
#define MINESVPN_COMMON_H

#include <chrono>
#include <vector>
#include <iostream>

enum logLevels {
  ERROR, WARNING, INFO, DEBUG
};

inline std::ostream &
operator<<(std::ostream &os, const logLevels &level) {
  switch (level) {
    case ERROR:
      os << "ERROR";
      break;
    case WARNING:
      os << "WARNING";
      break;
    case DEBUG:
      os << "DEBUG";
      break;
    default:
      os << "Unknown";
      break;
  }
  return os;
}


enum sendingStrategy {
  BURST, UNIFORM
};

inline std::ostream &
operator<<(std::ostream &os, const sendingStrategy &strategy) {
  switch (strategy) {
    case BURST:
      os << "BURST";
      break;
    case UNIFORM:
      os << "UNIFORM";
      break;
    default:
      os << "Unknown";
      break;
  }
  return os;
}

enum connectionStatus {
  SYN, ONGOING, FIN
};

inline std::ostream &
operator<<(std::ostream &os, const connectionStatus &status) {
  switch (status) {
    case SYN:
      os << "SYN";
      break;
    case ONGOING:
      os << "ONGOING";
      break;
    case FIN:
      os << "FIN";
      break;
    default:
      os << "Unknown";
      break;
  }
  return os;
}


struct addressPair {
  char clientAddress[256] = ""; // Max length of hostname is 253 ASCII chars
  char clientPort[6] = "";
  char serverAddress[16] = "";
  char serverPort[6] = "";
};

#endif //MINESVPN_COMMON_H
