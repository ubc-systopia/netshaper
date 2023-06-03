//
// Created by Rut Vora
//

#ifdef RECORD_STATS

#include <chrono>
#include <vector>

#ifndef MINESVPN_PERFEVAL_H
#define MINESVPN_PERFEVAL_H

struct shaperStats {
  uint64_t min = UINT64_MAX;
  uint64_t minIndex = 0;
  uint64_t maxIndex = 0;
  uint64_t max = 0;
  uint64_t count = 0;
  double average = 0;
  double variance = 0;
  double M2 = 0.0;
};
enum statElem {
  DECISION, PREP, ENQUEUE, DECISION_PREP, LOOP
};

inline std::ostream &
operator<<(std::ostream &os, const shaperStats &stats) {
  os << "{\n";
  os << "\"min\": " << stats.min << ",\n";
  os << "\"max\": " << stats.max << ",\n";
  os << "\"minIndex\": " << stats.minIndex << ",\n";
  os << "\"maxIndex\": " << stats.maxIndex << ",\n";
  os << "\"count\": " << stats.count << ",\n";
  os << "\"average\": " << stats.average << ",\n";
  os << "\"variance\": " << stats.variance << ",\n";
  os << "},\n";
  return os;
}

inline std::ostream &
operator<<(std::ostream &os, const statElem &elem) {
  switch (elem) {
    case DECISION:
      os << "\"Decision\": ";
      break;
    case PREP:
      os << "\"Prep\": ";
      break;
    case ENQUEUE:
      os << "\"Enqueue\": ";
      break;
    case DECISION_PREP:
      os << "\"Decision + Prep\": ";
      break;
    case LOOP:
      os << "\"Loop\": ";
      break;
  }
  return os;
}

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

#endif  //MINESVPN_PERFEVAL_H

#endif
