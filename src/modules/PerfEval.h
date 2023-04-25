//
// Created by Rut Vora
//
#include <chrono>
#include <vector>

#ifndef MINESVPN_PERFEVAL_H
#define MINESVPN_PERFEVAL_H

std::vector<std::chrono::time_point<std::chrono::steady_clock>> tcpIn{};
std::vector<std::chrono::time_point<std::chrono::steady_clock>> quicIn{};
std::vector<std::chrono::time_point<std::chrono::steady_clock>> tcpOut{};
std::vector<std::chrono::time_point<std::chrono::steady_clock>> quicOut{};

#endif //MINESVPN_PERFEVAL_H
