// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // close()
#include <arpa/inet.h>          // inet_addr(), htons()
#include <sys/socket.h>         // socket(), connect(), send(), recv()
#include "cJSON.h"
#include "board.h"

#define SIZE 8

char username[32];
int sock;

int dr[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
int dc[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };

void send_json(cJSON* json) {
    char* text = cJSON_PrintUnformatted(json);
    send(sock, text, strlen(text), 0);
    send(sock, "\n", 1, 0);
    free(text);
}

void move_generate(char board[SIZE][SIZE], int* sx, int* sy, int* tx, int* ty, char token) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != token) continue;

            for (int d = 0; d < 8; d++) {
                int nr = r + dr[d], nc = c + dc[d];
                int jr = r + 2 * dr[d], jc = c + 2 * dc[d];

                if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE && board[nr][nc] == '.') {
                    *sx = r; *sy = c;
                    *tx = nr; *ty = nc;
                    return;
                }
                if (jr >= 0 && jr < SIZE && jc >= 0 && jc < SIZE && board[jr][jc] == '.') {
                    *sx = r; *sy = c;
                    *tx = jr; *ty = jc;
                    return;
                }
            }
        }
    }
    *sx = *sy = *tx = *ty = 0;
}

void print_board(cJSON* board_array) {
    char board[8][9];
    for (int i = 0; i < SIZE; i++) {
        const char* row = cJSON_GetArrayItem(board_array, i)->valuestring;
        strncpy(board[i], row, 9);
        printf("%s\n", board[i]);
    }
    update_led_board(board);
}

void handle_turn(cJSON* json) {
    cJSON* board_arr = cJSON_GetObjectItem(json, "board");

    printf("\n=== Current Board ===\n");
    print_board(board_arr);

    char board[SIZE][SIZE];
    for (int i = 0; i < SIZE; i++) {
        const char* row = cJSON_GetArrayItem(board_arr, i)->valuestring;
        for (int j = 0; j < SIZE; j++) board[i][j] = row[j];
    }

    char token = (strcmp(username, "Alice") == 0) ? 'R' : 'B';
    int sx, sy, tx, ty;
    move_generate(board, &sx, &sy, &tx, &ty, token);

    cJSON* move = cJSON_CreateObject();
    cJSON_AddStringToObject(move, "type", "move");
    cJSON_AddStringToObject(move, "username", username);
    cJSON_AddNumberToObject(move, "sx", sx);
    cJSON_AddNumberToObject(move, "sy", sy);
    cJSON_AddNumberToObject(move, "tx", tx);
    cJSON_AddNumberToObject(move, "ty", ty);
    send_json(move);
    cJSON_Delete(move);
}

int main(int argc, char* argv[]) {
    printf("[DEBUG] Program started\n"); fflush(stdout);

    if (argc != 7) {
        printf("Usage: %s -ip <ip> -port <port> -username <name>\n", argv[0]);
        return 1;
    }

    char* ip = NULL;
    int port = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0) ip = argv[++i];
        else if (strcmp(argv[i], "-port") == 0) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-username") == 0) strncpy(username, argv[++i], sizeof(username));
    }

    printf("[DEBUG] Parsed args - IP: %s, PORT: %d, USERNAME: %s\n", ip, port, username); fflush(stdout);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    printf("[DEBUG] Attempting to connect to server...\n"); fflush(stdout);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != 0) {
        perror("connect() failed");
        return 1;
    }

    printf("[CLIENT] Connected to server.\n");

    cJSON* reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "type", "register");
    cJSON_AddStringToObject(reg, "username", username);
    send_json(reg);
    cJSON_Delete(reg);

    char recv_buf[4096] = "";
    char buffer[2048];
    int len;

    while (1) {
        len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            printf("[ERROR] Disconnected or recv failed.\n");
            break;
        }
        buffer[len] = '\0';
        strcat(recv_buf, buffer);

        char* newline;
        while ((newline = strchr(recv_buf, '\n')) != NULL) {
            *newline = '\0';

            cJSON* msg = cJSON_Parse(recv_buf);
            if (!msg) {
                printf("[ERROR] Failed to parse JSON.\n");
                memmove(recv_buf, newline + 1, strlen(newline + 1) + 1);
                continue;
            }

            cJSON* typeItem = cJSON_GetObjectItem(msg, "type");
            if (!cJSON_IsString(typeItem)) {
                cJSON_Delete(msg);
                memmove(recv_buf, newline + 1, strlen(newline + 1) + 1);
                continue;
            }
            const char* type = typeItem->valuestring;

            if (strcmp(type, "register_ack") == 0) {
                printf("[CLIENT] Registered successfully.\n");
            }
            else if (strcmp(type, "game_start") == 0) {
                printf("[CLIENT] Game started.\n");
            }
            else if (strcmp(type, "your_turn") == 0) {
                printf("[CLIENT] It's your turn.\n");
                handle_turn(msg);
            }
            else if (strcmp(type, "move_ok") == 0) {
                printf("[CLIENT] Move accepted.\n");
            }
            else if (strcmp(type, "invalid_move") == 0) {
                printf("[CLIENT] Invalid move.\n");
            }
            else if (strcmp(type, "pass") == 0) {
                printf("[CLIENT] Turn passed.\n");
            }
            else if (strcmp(type, "game_over") == 0) {
                printf("[CLIENT] Game Over! Scores:\n");
                cJSON* scores = cJSON_GetObjectItem(msg, "scores");

                int my_score = 0, opp_score = 0;
                const char* opponent = NULL;

                cJSON* player = scores->child;
                while (player) {
                    printf("%s: %d\n", player->string, player->valueint);

                    if (strcmp(player->string, username) == 0) {
                        my_score = player->valueint;
                    }
                    else {
                        opp_score = player->valueint;
                        opponent = player->string;
                    }
                    player = player->next;
                }

                if (my_score > opp_score) {
                    printf("%s wins!\n", username);
                }
                else if (my_score < opp_score) {
                    printf("%s wins!\n", opponent);
                }
                else {
                    printf("Draw\n");
                }
                cJSON_Delete(msg);
                break;
            }

            cJSON_Delete(msg);
            memmove(recv_buf, newline + 1, strlen(newline + 1) + 1);
        }
    }

    close(sock);
    return 0;
}
