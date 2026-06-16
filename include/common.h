#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

// --- KONFIGURACJA ---
#define MULTICAST_ADDR "239.255.42.99"
#define MULTICAST_PORT 5555
#define BOARD_SIZE 3

typedef struct {
    int players[2];      // FD graczy: [0] to X, [1] to O
    char board[9];       // Plansza 3x3 jako płaska tablica
    int current_turn;    // 0 lub 1
} game_session_t;

// --- TYPY KOMUNIKATÓW (TLV Type) ---
typedef enum {
    MSG_LOGIN_REQ = 1,     // Zmienione indeksy po usunięciu Discovery 
    MSG_LOGIN_RESP,
    MSG_JOIN_QUEUE,
    MSG_GAME_START,
    MSG_MOVE_REQ,
    MSG_BOARD_UPDATE,
    MSG_GAME_OVER,
    MSG_RANKING_REQ,
    MSG_ERROR = 99
} msg_type_t;

// --- NAGŁÓWEK TLV ---
#pragma pack(push, 1)
typedef struct {
    uint8_t type;          // Typ z msg_type_t
    uint16_t length;       // Długość pola Value
} tlv_header_t;

// --- REKORD UŻYTKOWNIKA (do pliku users.dat) ---
typedef struct {
    char username[32];
    uint8_t password_hash[32]; 
    uint32_t wins;
    uint32_t losses;
    uint32_t draws;
} user_record_t;
#pragma pack(pop)

#endif