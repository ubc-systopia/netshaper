//
// Created by Rut Vora
//

#ifndef MINESVPN_UNSHAPED_H
#define MINESVPN_UNSHAPED_H


#include "Base.h"

class Unshaped : public Base {
protected:
  __useconds_t shapedProcessLoopInterval;

/**
 * @brief Check queues for data periodically and send it to corresponding socket
 * @param interval The interval at which the queues are checked
 */
  [[noreturn]] virtual void checkQueuesForData(__useconds_t interval) = 0;
};


#endif //MINESVPN_UNSHAPED_H
