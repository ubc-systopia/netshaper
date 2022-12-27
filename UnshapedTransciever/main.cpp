//
// Created by Rut Vora
//

#include "ProxyListener.h"

int main() {
  std::string remoteHost;
  int remotePort;

  std::cout << "Enter the IP address of the remote Host you want to proxy " <<
               "to:" << std::endl;
  std::cin >> remoteHost;

  std::cout << "Enter the port of the remote Host you want to proxy " <<
               "to:" << std::endl;
  std::cin >> remotePort;

  ProxyListener proxyListener(remoteHost, remotePort);
  proxyListener.startListening();
  return 0;
}