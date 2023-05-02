//
// Created by Rut Vora
//
#include <chrono>
#include <vector>

#ifndef MINESVPN_PERFEVAL_H
#define MINESVPN_PERFEVAL_H

#ifdef RECORD_STATS

std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut(128);
std::vector<std::vector<uint64_t>>  tcpSend(128);
std::vector<std::vector<uint64_t>> udpSend(1);

#endif

#endif //MINESVPN_PERFEVAL_H
