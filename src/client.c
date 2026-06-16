#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../include/common.h"
#include "../include/network.h"

// Port zdefiniowany w common.h lub tutaj jako fallback
#ifndef TCP_PORT
#define TCP_PORT 6000
#endif

// Kody kolorów ANSI
#define COLOR_RESET   "\033[0m"
#define COLOR_PURPLE  "\033[35m"
#define COLOR_ORANGE  "\033[38;5;208m"

// Minimalna długość loginu i hasła
#define MIN_CREDENTIALS_LENGTH 6

void clear_screen() {
    system("clear");
}

void print_logo() {
    printf(COLOR_PURPLE);
    printf("\n");
    printf("***Tic-Tac-Toe***");
    printf("\n");
    printf(COLOR_RESET);
}

void draw_board(char *board, char my_symbol) {
    printf("\nSTAN PLANSZY           LEGENDA PÓL\n");
    for (int i = 0; i < 3; i++) {
        printf("  ");
        for (int j = 0; j < 3; j++) {
            int idx = i * 3 + j;
            char c = board[idx];

            // Kolorowanie symboli
            if (c == 'X') {
                printf(COLOR_ORANGE "%c" COLOR_RESET, c);
            } else if (c == 'O') {
                printf(COLOR_PURPLE "%c" COLOR_RESET, c);
            } else {
                printf("%c", c);
            }

            if (j < 2) printf(" | ");
        }

        printf("            %d | %d | %d\n", i*3, i*3+1, i*3+2);

        if (i < 2) {
            printf(" ---+---+---          ---+---+---\n");
        }
    }
    printf("\n");
}

void play_game(int sock) {
    tlv_header_t h;
    void* val = NULL;
    int game_active = 1;
    char my_symbol = '?';

    clear_screen();
    printf("Oczekiwanie na sygnał startu od serwera...\n");

    while (game_active && recv_tlv(sock, &h, &val) == 0) {
        switch (h.type) {
            case MSG_GAME_START:
                my_symbol = ((char*)val)[0];
                clear_screen();
                if (my_symbol == 'X') {
                    printf("Gra rozpoczęta! Twój symbol to: " COLOR_ORANGE "**%c**" COLOR_RESET "\n", my_symbol);
                } else {
                    printf("Gra rozpoczęta! Twój symbol to: " COLOR_PURPLE "**%c**" COLOR_RESET "\n", my_symbol);
                }
                break;

            case MSG_BOARD_UPDATE:
                clear_screen();
                if (my_symbol == 'X') {
                    printf("Grasz jako: " COLOR_ORANGE "%c" COLOR_RESET "\n", my_symbol);
                } else {
                    printf("Grasz jako: " COLOR_PURPLE "%c" COLOR_RESET "\n", my_symbol);
                }
                draw_board((char*)val, my_symbol);
                break;

            case MSG_MOVE_REQ:
                if (my_symbol == 'X') {
                    printf("Twoja tura (" COLOR_ORANGE "%c" COLOR_RESET "). Podaj pole (0-8): ", my_symbol);
                } else {
                    printf("Twoja tura (" COLOR_PURPLE "%c" COLOR_RESET "). Podaj pole (0-8): ", my_symbol);
                }
                char move;
                if (scanf(" %c", &move) != 1) break;
                send_tlv(sock, MSG_MOVE_REQ, 1, &move);
                break;

            case MSG_GAME_OVER:
                printf("\n*******************************");
                printf("\n*** KONIEC GRY: %-10s ***", (char*)val);
                printf("\n*******************************\n");
                printf("\nAby zagrać ponownie, uruchom program jeszcze raz!\n");
                printf("Naciśnij Enter, aby zakończyć...");
                while (getchar() != '\n');
                getchar();
                game_active = 0;
                break;

            case MSG_ERROR:
                printf("[Błąd]: %s\n", (char*)val);
                break;
        }
        if (val) { free(val); val = NULL; }
    }
}

int validate_credentials(const char* login, const char* password) {
    if (strlen(login) < MIN_CREDENTIALS_LENGTH) {
        printf("\n[Błąd] Login musi mieć co najmniej %d znaków!\n", MIN_CREDENTIALS_LENGTH);
        return 0;
    }
    if (strlen(password) < MIN_CREDENTIALS_LENGTH) {
        printf("\n[Błąd] Hasło musi mieć co najmniej %d znaków!\n", MIN_CREDENTIALS_LENGTH);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    clear_screen();
    if (argc != 2) {
        printf("Użycie: %s <IP_SERWERA>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        printf("[Błąd] Niepoprawny adres IP.\n");
        return 1;
    }

    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    if (connect(tcp_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Błąd] Nie można połączyć się z serwerem (sprawdź czy serwer działa)");
        close(tcp_sock);
        return 1;
    }

    printf("[TCP] Połączono pomyślnie z %s\n", argv[1]);
    sleep(1);

    int choice;
    char user[32], pass[32], payload[70];
    int logged_in = 0;

    while (1) {
        clear_screen();
        print_logo();

        printf("\n--- MENU GŁÓWNE ---\n");
        printf("1. Logowanie / Rejestracja\n");
        printf("2. Szukaj Gry\n");
        printf("3. Ranking\n");
        printf("4. Wyjście\n");
        printf("\nWybór: ");

        if (scanf("%d", &choice) != 1) {
            // Czyszczenie bufora w przypadku wpisania liter
            while (getchar() != '\n');
            printf("\n[Błąd] Nieprawidłowy wybór!\n");
            sleep(1);
            continue;
        }

        if (choice == 1) {
            clear_screen();
            print_logo();
            printf("\n--- LOGOWANIE / REJESTRACJA ---\n");
            printf("Login: ");
            scanf("%s", user);
            printf("Hasło: ");
            scanf("%s", pass);

            // Walidacja długości
            if (!validate_credentials(user, pass)) {
                printf("\nNaciśnij Enter, aby kontynuować...");
                while (getchar() != '\n'); // Wyczyść bufor
                getchar();
                continue;
            }

            // Tworzenie ramki login:haslo
            snprintf(payload, sizeof(payload), "%s:%s", user, pass);
            send_tlv(tcp_sock, MSG_LOGIN_REQ, strlen(payload) + 1, payload);

            tlv_header_t h; void* v = NULL;
            if (recv_tlv(tcp_sock, &h, &v) == 0) {
                if (h.type == MSG_LOGIN_RESP) {
                    char* response = (char*)v;
                    if (strcmp(response, "OK") == 0) {
                        printf("\n✓ Zalogowano pomyślnie!\n");
                        logged_in = 1;
                    } else if (strcmp(response, "RG") == 0) {
                        printf("\n✓ Zarejestrowano i zalogowano pomyślnie!\n");
                        logged_in = 1;
                    } else if (strstr(response, "min 6") != NULL) {
                        printf("\n✗ %s\n", response);
                    } else {
                        printf("\n✗ Nieprawidłowe hasło!\n");
                    }
                }
                if (v) free(v);
            }
            printf("\nNaciśnij Enter, aby kontynuować...");
            while (getchar() != '\n'); // Wyczyść bufor
            getchar();
        }
        else if (choice == 2) {
            if (!logged_in) {
                printf("\n[Błąd] Musisz się najpierw zalogować!\n");
                sleep(2);
                continue;
            }

            clear_screen();
            send_tlv(tcp_sock, MSG_JOIN_QUEUE, 0, NULL);
            printf("Dołączono do kolejki. Czekaj na przeciwnika...\n");
            play_game(tcp_sock);
            break; // Wyjście po zakończeniu gry
        }
        else if (choice == 3) {
            clear_screen();
            print_logo();
            send_tlv(tcp_sock, MSG_RANKING_REQ, 0, NULL);
            tlv_header_t h; void* v = NULL;
            if (recv_tlv(tcp_sock, &h, &v) == 0) {
                printf("%s\n", (char*)v);
                if (v) free(v);
            }
            printf("\nNaciśnij Enter, aby wrócić do menu...");
            while (getchar() != '\n'); // Wyczyść bufor
            getchar();
        }
        else if (choice == 4) {
            clear_screen();
            printf("\nDziękujemy za grę! Do zobaczenia!\n\n");
            break;
        }
        else {
            printf("\n[Błąd] Nieprawidłowy wybór!\n");
            sleep(1);
        }
    }

    close(tcp_sock);
    return 0;
}
