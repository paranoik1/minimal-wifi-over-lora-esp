#ifndef TRANSPORT_H
#define TRANSPORT_H


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void init_transport();


size_t receive_packet(uint8_t* buff, size_t max_len);

void send_packet(const uint8_t* packet, size_t size_packet);


#ifdef __cplusplus
}
#endif

#endif
