//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_H
#define MINESVPN_SHAPED_H


#include <cstdlib>
#include <unordered_map>
#include "msquic.hpp"
#include "helpers.h"
#include "Base.h"

class Shaped : public Base {
protected:
  __useconds_t unshapedProcessLoopInterval;
  std::unordered_map<helpers::QueuePair, MsQuicStream *,
      helpers::QueuePairHash> *queuesToStream;
  std::unordered_map<MsQuicStream *, helpers::QueuePair> *streamToQueues;
  std::unordered_map<MsQuicStream *, QUIC_UINT62> *streamToID;

  std::shared_mutex mapLock;

  MsQuicStream *controlStream;
  MsQuicStream *dummyStream;

  NoiseGenerator *noiseGenerator;

  /**
   * @brief Send dummy of given size on the dummy stream
   * @param dummySize The #bytes to send
   * @return The prepared buffer (stream, buffer and size)
   */
  virtual helpers::PreparedBuffer prepareDummy(size_t dummySize) = 0;

  /**
 * @brief Send data to the receiving middleBox
 * @param dataSize The number of bytes to send out
 * @return Vector containing the prepared buffer (stream, buffer and size)
 */
  virtual std::vector<helpers::PreparedBuffer> prepareData(size_t dataSize) = 0;


  /**
 * @brief Handle receiving control messages from the other middleBox
 * @param ctrlStream The control stream this message was received on
 * @param buffer The buffer containing the messages
 * @param length The length of the buffer
 */
  virtual void handleControlMessages(MsQuicStream *ctrlStream,
                                     uint8_t *buffer, size_t length) = 0;

  /**
   * @brief Find a stream by it's ID
   * @param ID The ID to look for
   * @return The stream pointer corresponding to that ID
   */
  virtual MsQuicStream *findStreamByID(QUIC_UINT62 ID) = 0;

  /**
 * @brief Function that is called when a response is received
 * @param stream The stream on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the received response
 */
  virtual void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
  length) = 0;
};


#endif //MINESVPN_SHAPED_H
