#include "../include/network.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// Pomocnicza funkcja do odbierania DOKŁADNIE N bajtów
// TCP może podzielić dużą paczkę danych na kilka mniejszych
static ssize_t read_n_bytes(int fd, void *buf, size_t n) {
    size_t total_read = 0;
    char *ptr = (char *)buf;
    while (total_read < n) {
        ssize_t n_read = recv(fd, ptr + total_read, n - total_read, 0);
        if (n_read <= 0) return n_read; // Błąd lub rozłączenie
        total_read += n_read;
    }
    return total_read;
}

int send_tlv(int socket_fd, uint8_t type, uint16_t length, const void* value) {
    tlv_header_t header;
    header.type = type;
    header.length = htons(length); // Konwersja na Network Byte Order (Big-Endian)

    // 1. Wysyłamy nagłówek
    if (send(socket_fd, &header, sizeof(header), 0) < 0) return -1;

    // 2. Wysyłamy dane (jeśli istnieją)
    if (length > 0 && value != NULL) {
        if (send(socket_fd, value, length, 0) < 0) return -1;
    }
    return 0;
}

int recv_tlv(int socket_fd, tlv_header_t* out_header, void** out_value) {
    // 1. Odbierz nagłówek
    if (read_n_bytes(socket_fd, out_header, sizeof(tlv_header_t)) <= 0) {
        return -1;
    }

    // Konwersja długości z formatu sieciowego na format procesora
    out_header->length = ntohs(out_header->length);

    // 2. Jeśli jest treść (Value), alokuj pamięć i odbierz ją
    if (out_header->length > 0) {
        *out_value = malloc(out_header->length);
        if (*out_value == NULL) return -1;

        if (read_n_bytes(socket_fd, *out_value, out_header->length) <= 0) {
            free(*out_value);
            return -1;
        }
    } else {
        *out_value = NULL;
    }

    return 0;
}