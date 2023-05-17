//
// Created by Rut Vora
//

#ifdef RECORD_STATS

#include <chrono>
#include <vector>

#ifndef MINESVPN_PERFEVAL_H
#define MINESVPN_PERFEVAL_H

std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut(128);
std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut(128);
std::vector<std::vector<uint64_t>> tcpSend(128);
std::vector<std::vector<uint64_t>> quicSend(128);
std::vector<std::pair<uint64_t, uint64_t>> timeDPDecisions{}; // size, time
std::vector<std::pair<uint64_t, uint64_t>> timeDataPrep{}; // size, time
std::vector<std::pair<uint64_t, uint64_t>> timeDataEnqueue{}; // size, time

#endif  //MINESVPN_PERFEVAL_H

#endif
