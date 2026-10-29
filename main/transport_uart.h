#ifndef TRANSPORT_H
#define TRANSPORT_H


#include "packet.h"


#ifdef __cplusplus
extern "C" {
#endif
void init_transport();


Packet* transport_receive_packet();

void transport_send_packet(Packet *packet);


#ifdef __cplusplus
}
#endif

#endif
