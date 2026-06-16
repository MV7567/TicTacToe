#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/common.h"

#define USERS_DB "data/users.dat"

// Prosta, ale efektywna funkcja haszująca DJB2
uint32_t hash_password(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

// Szuka użytkownika w pliku binarnym
int find_user(const char* username, user_record_t* found_user) {
    FILE *f = fopen(USERS_DB, "rb");
    if (!f) return 0;

    user_record_t temp;
    while (fread(&temp, sizeof(user_record_t), 1, f)) {
        if (strcmp(temp.username, username) == 0) {
            if (found_user) *found_user = temp;
            fclose(f);
            return 1; // Znaleziono
        }
    }
    fclose(f);
    return 0; // Nie znaleziono
}

// Rejestruje nowego użytkownika
int register_user(const char* username, const char* password) {
    if (find_user(username, NULL)) return -1; // Już istnieje

    user_record_t new_user = {0};
    strncpy(new_user.username, username, 31);
    
    // W naszym projekcie hash przechowujemy w polu password_hash
    // Dla uproszczenia używamy uint32_t rzutowanego na tablicę bajtów
    uint32_t hash = hash_password(password);
    memcpy(new_user.password_hash, &hash, sizeof(uint32_t));

    FILE *f = fopen(USERS_DB, "ab"); // Append binary
    if (!f) return -2;

    fwrite(&new_user, sizeof(user_record_t), 1, f);
    fclose(f);
    return 0;
}


#include <stdlib.h> // Dla qsort

// Funkcja pomocnicza do porównywania graczy (dla qsort)
// Sortujemy malejąco według liczby zwycięstw
int compare_users(const void *a, const void *b) {
    user_record_t *userA = (user_record_t *)a;
    user_record_t *userB = (user_record_t *)b;
    return (userB->wins - userA->wins);
}

// 1. Aktualizacja statystyk po grze
void update_user_stats(const char* username, int result) {
    // result: 1 = win, 0 = draw, -1 = loss
    FILE *f = fopen(USERS_DB, "r+b"); // Tryb odczytu i zapisu
    if (!f) return;

    user_record_t temp;
    while (fread(&temp, sizeof(user_record_t), 1, f)) {
        if (strcmp(temp.username, username) == 0) {
            if (result == 1) temp.wins++;
            else if (result == 0) temp.draws++;
            else if (result == -1) temp.losses++;

            // Cofnij wskaźnik pliku o jeden rekord i nadpisz go
            fseek(f, -sizeof(user_record_t), SEEK_CUR);
            fwrite(&temp, sizeof(user_record_t), 1, f);
            break;
        }
    }
    fclose(f);
}

// 2. Pobieranie sformatowanego rankingu
int get_ranking_string(char* buffer, size_t max_len) {
    FILE *f = fopen(USERS_DB, "rb");
    if (!f) return 0;

    // Policz ilu mamy użytkowników
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    int count = file_size / sizeof(user_record_t);
    rewind(f);

    if (count == 0) {
        snprintf(buffer, max_len, "Ranking jest pusty.");
        fclose(f);
        return strlen(buffer);
    }

    // Wczytaj wszystkich do tablicy, aby ich posortować
    user_record_t *list = malloc(count * sizeof(user_record_t));
    fread(list, sizeof(user_record_t), count, f);
    fclose(f);

    qsort(list, count, sizeof(user_record_t), compare_users);

    // Budowanie tabeli tekstowej
    int offset = snprintf(buffer, max_len, "\n--- RANKING GRACZY ---\n%-15s | %s | %s | %s\n", 
                          "Gracz", "W", "P", "R");
    strncat(buffer, "---------------------------------------\n", max_len - offset);
    offset = strlen(buffer);

    for (int i = 0; i < count && i < 10; i++) { // Top 10
        int written = snprintf(buffer + offset, max_len - offset, "%-15s | %d | %d | %d\n",
                               list[i].username, list[i].wins, list[i].losses, list[i].draws);
        offset += written;
    }

    free(list);
    return offset; // Zwraca długość napisu
}