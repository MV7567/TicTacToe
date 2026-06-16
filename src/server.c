#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "../include/common.h"
#include "../include/network.h"

// Port jest zdefiniowany w common.h (6000)
#ifndef TCP_PORT
#define TCP_PORT 6000
#endif

// --- Synchronizacja i Kolejka ---
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

int waiting_player_fd = -1;
char waiting_player_name[32] = "";

// --- Deklaracje funkcji zewnętrznych ---
uint32_t hash_password(const char *str);
int find_user(const char* username, user_record_t* found_user);
int register_user(const char* username, const char* password);
void update_user_stats(const char* username, int result);
int get_ranking_string(char* buffer, size_t max_len);

// --- Rozszerzona struktura sesji dla serwera ---
typedef struct {
    int fds[2];           // [0]=X, [1]=O
    char names[2][32];    // Nazwy użytkowników do statystyk
    char board[9];
    int current_turn;
} server_game_t;

// --- Logika Gry: Pomocnicze ---
int check_win(char *b) {
    int wins[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6},
        {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}
    };
    for(int i=0; i<8; i++) {
        if(b[wins[i][0]] != ' ' && b[wins[i][0]] == b[wins[i][1]] && b[wins[i][0]] == b[wins[i][2]]) return 1;
    }
    return 0;
}

int is_board_full(char *b) {
    for(int i=0; i<9; i++) if(b[i] == ' ') return 0;
    return 1;
}

// --- Wątek Logiki Gry ---
void* game_logic_thread(void* arg) {
    server_game_t *s = (server_game_t*)arg;
    tlv_header_t h;
    void* val = NULL;
    int game_running = 1;

    // Start: Poinformuj graczy i wyślij planszę
    send_tlv(s->fds[0], MSG_GAME_START, 2, "X");
    send_tlv(s->fds[1], MSG_GAME_START, 2, "O");
    send_tlv(s->fds[0], MSG_BOARD_UPDATE, 9, s->board);
    send_tlv(s->fds[1], MSG_BOARD_UPDATE, 9, s->board);

    while (game_running) {
        int active = s->current_turn;
        int opponent = 1 - s->current_turn;

        send_tlv(s->fds[active], MSG_MOVE_REQ, 0, NULL);

        if (recv_tlv(s->fds[active], &h, &val) != 0) {
            send_tlv(s->fds[opponent], MSG_GAME_OVER, 14, "OPPONENT_LEFT");
            pthread_mutex_lock(&db_mutex);
            update_user_stats(s->names[opponent], 1);
            update_user_stats(s->names[active], -1);
            pthread_mutex_unlock(&db_mutex);
            break;
        }

        if (h.type == MSG_MOVE_REQ) {
            int move_idx = ((char*)val)[0] - '0';
            if (move_idx >= 0 && move_idx < 9 && s->board[move_idx] == ' ') {
                s->board[move_idx] = (active == 0) ? 'X' : 'O';

                send_tlv(s->fds[0], MSG_BOARD_UPDATE, 9, s->board);
                send_tlv(s->fds[1], MSG_BOARD_UPDATE, 9, s->board);

                if (check_win(s->board)) {
                    send_tlv(s->fds[active], MSG_GAME_OVER, 8, "VICTORY");
                    send_tlv(s->fds[opponent], MSG_GAME_OVER, 8, "DEFEAT");
                    pthread_mutex_lock(&db_mutex);
                    update_user_stats(s->names[active], 1);
                    update_user_stats(s->names[opponent], -1);
                    pthread_mutex_unlock(&db_mutex);
                    game_running = 0;
                } else if (is_board_full(s->board)) {
                    send_tlv(s->fds[0], MSG_GAME_OVER, 5, "DRAW");
                    send_tlv(s->fds[1], MSG_GAME_OVER, 5, "DRAW");
                    pthread_mutex_lock(&db_mutex);
                    update_user_stats(s->names[0], 0);
                    update_user_stats(s->names[1], 0);
                    pthread_mutex_unlock(&db_mutex);
                    game_running = 0;
                }
                s->current_turn = opponent;
            } else {
                send_tlv(s->fds[active], MSG_ERROR, 13, "INVALID_MOVE");
            }
        }
        if (val) { free(val); val = NULL; }
    }

    close(s->fds[0]);
    close(s->fds[1]);
    free(s);
    return NULL;
}

void start_game_session(int p1, int p2, const char* n1, const char* n2) {
    server_game_t *s = malloc(sizeof(server_game_t));
    s->fds[0] = p1; s->fds[1] = p2;
    strncpy(s->names[0], n1, 31); strncpy(s->names[1], n2, 31);
    s->current_turn = 0;
    memset(s->board, ' ', 9);

    pthread_t tid;
    pthread_create(&tid, NULL, game_logic_thread, s);
    pthread_detach(tid);
}

// --- Client Handler TCP ---
void* client_handler(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    tlv_header_t h;
    void* val = NULL;
    int logged = 0;
    int in_game = 0;
    char user[32] = "";

    while (recv_tlv(client_fd, &h, &val) == 0) {
        if (h.type == MSG_LOGIN_REQ) {
            char* p = strchr((char*)val, ':');
            if (p) {
                *p = '\0';
                p++;
                pthread_mutex_lock(&db_mutex);
                user_record_t rec;
                if (find_user((char*)val, &rec)) {
                    uint32_t incoming_hash = hash_password(p);
                    uint32_t stored_hash;
                    memcpy(&stored_hash, rec.password_hash, sizeof(uint32_t));

                    if (incoming_hash == stored_hash) {
                        send_tlv(client_fd, MSG_LOGIN_RESP, 3, "OK");
                        logged = 1;
                        strncpy(user, (char*)val, 31);
                        printf("[Auth] Gracz %s zalogowany.\n", user);
                    } else {
                        send_tlv(client_fd, MSG_LOGIN_RESP, 5, "FAIL");
                    }
                } else {
                    // FIX: Po rejestracji gracz jest od razu zalogowany
                    register_user((char*)val, p);
                    send_tlv(client_fd, MSG_LOGIN_RESP, 3, "RG");
                    logged = 1;
                    strncpy(user, (char*)val, 31);
                    printf("[Auth] Nowy gracz %s zarejestrowany i zalogowany.\n", user);
                }
                pthread_mutex_unlock(&db_mutex);
            }
        }
        else if (h.type == MSG_RANKING_REQ) {
            char r_buf[2048] = {0};
            pthread_mutex_lock(&db_mutex);
            int len = get_ranking_string(r_buf, sizeof(r_buf));
            pthread_mutex_unlock(&db_mutex);
            send_tlv(client_fd, MSG_RANKING_REQ, len + 1, r_buf);
        }
        else if (h.type == MSG_JOIN_QUEUE) {
            if (!logged) {
                send_tlv(client_fd, MSG_ERROR, 21, "Musisz sie zalogowac!");
                if (val) free(val);
                continue;
            }

            pthread_mutex_lock(&queue_mutex);
            if (waiting_player_fd == -1) {
                waiting_player_fd = client_fd;
                strncpy(waiting_player_name, user, 31);
                pthread_mutex_unlock(&queue_mutex);
                printf("[Queue] Gracz %s (FD:%d) czeka na przeciwnika...\n", user, client_fd);
                in_game = 1;
                if (val) free(val);
                break; // Socket przechodzi do watku gry
            } else {
                int opp_fd = waiting_player_fd;
                char opp_name[32];
                strncpy(opp_name, waiting_player_name, 31);
                waiting_player_fd = -1;
                pthread_mutex_unlock(&queue_mutex);

                printf("[Match] Parowanie %s (FD:%d) z %s (FD:%d)\n", opp_name, opp_fd, user, client_fd);
                start_game_session(opp_fd, client_fd, opp_name, user);
                in_game = 1;
                if (val) free(val);
                break; // Sockets przechodza do watku gry
            }
        }
        if (val) { free(val); val = NULL; }
    }

    if (!in_game) {
        printf("[Serwer] Rozlaczono gracza %s\n", user);
        close(client_fd);
    }
    return NULL;
}

int main() {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(TCP_PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(sfd, 10);
    printf("[Serwer] Uruchomiony na porcie %d. Czekam na graczy...\n", TCP_PORT);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int* cfd = malloc(sizeof(int));
        *cfd = accept(sfd, (struct sockaddr*)&caddr, &clen);

        printf("[Serwer] Nowe polaczenie z %s\n", inet_ntoa(caddr.sin_addr));

        pthread_t ct;
        pthread_create(&ct, NULL, client_handler, cfd);
        pthread_detach(ct);
    }
    return 0;
}
