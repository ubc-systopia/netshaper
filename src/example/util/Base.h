//
// Created by Rut Vora
//

#ifndef MINESVPN_BASE_H
#define MINESVPN_BASE_H


#include <string>
#include <mutex>
#include "../../modules/Common.h"
#include "helpers.h"

class Base {
protected:
  std::string appName;
  logLevels logLevel;
  std::unordered_map<uint64_t, connectionStatus> *pendingSignal;
  std::mutex logWriter;

  class helpers::SignalInfo *sigInfo;

  std::mutex readLock;
  std::mutex writeLock;

  /**
   * @brief Log the comments passed by various functions
   * @param level The level of the comment passed by the function
   * @param log The string containing the actual log
   */
  virtual void log(logLevels level, const std::string &log) = 0;

  /**
   * @brief Signal the shaped process
   * @param queueID The queue to send the signal about
   * @param connStatus The connection status (should be FIN)
   */
  virtual void updateConnectionStatus(uint64_t queueID,
                                      connectionStatus connStatus) = 0;

  /**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  virtual void initialiseSHM(int numStreams, size_t queueSize) = 0;

public:
  /**
* @brief Handle signal from the shaped process regarding a new client
*/
  virtual void getUpdatedConnectionStatus() = 0;

  Base() : logLevel(ERROR), pendingSignal(nullptr), sigInfo(nullptr) {};
};


#endif //MINESVPN_BASE_H
