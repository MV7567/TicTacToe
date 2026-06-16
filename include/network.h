#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"
#include <sys/socket.h>

// Wysyła kompletną ramkę TLV
int send_tlv(int socket_fd, uint8_t type, uint16_t length, const void* value);

// Odbiera nagłówek, a potem alokuje i odbiera Value
int recv_tlv(int socket_fd, tlv_header_t* out_header, void** out_value);

#endif