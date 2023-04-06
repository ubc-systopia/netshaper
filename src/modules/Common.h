//
// Created by Rut Vora
//

#ifndef MINESVPN_COMMON_H
#define MINESVPN_COMMON_H

enum logLevels {
  ERROR, WARNING, DEBUG
};

enum connectionStatus {
  SYN, ONGOING, FIN
};

struct addressPair {
  char clientAddress[256] = ""; // Max length of hostname is 253 ASCII chars
  char clientPort[6] = "";
  char serverAddress[16] = "";
  char serverPort[6] = "";
};

#endif //MINESVPN_COMMON_H