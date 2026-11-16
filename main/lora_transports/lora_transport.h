#ifndef TRANSPORT_H
#define TRANSPORT_H


#include "esp_err.h"
#include "packet.h"


#ifdef __cplusplus
extern "C" {
#endif

int get_transport_state();

int get_transport_edges();

void init_transport();


Packet* transport_receive_packet();

void transport_send_packet(Packet *packet);


esp_err_t lora_configure();


#ifdef __cplusplus
}
#endif

#endif
