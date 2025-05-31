#define _CRT_SECURE_NO_WARNINGS //VS에서 발생하는 overflow 무시하기 위함
#define _WINSOCK_DEPRECATED_NO_WARNINGS //MS VS에서 소켓 프로그래밍 시 오래된 함수 사용 시 발생하는 경고 무시

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include "cJSON.h"

#pragma comment(lib, "ws2_32.lib") // VS에서 Winsock 라이브러리를 링커에 자동으로 연결하기 위함으로 linux 및 gcc에서는 에러가능성 존재

#define SIZE 8

char username[32];
SOCKET sock;


void send_json(cJSON* json) {
    char* text = cJSON_PrintUnformatted(json);
    send(sock, text, strlen(text), 0);
    send(sock, "\n", 1, 0); // Message delimiter for TCP
    free(text);
}

#define N 8

typedef struct {
    int sx, sy, tx, ty;
    int gain;
    int adj_my_cells;
    int is_clone;
} Move;

int dx[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
int dy[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

// Check if (x,y) is inside the 8x8 board
int in_bounds(int x, int y) {
    return x >= 0 && x < N && y >= 0 && y < N;
}

// Count number of opponent pieces flipped if I move to (tx,ty)
int count_flips(char board[8][9], char myColor, int tx, int ty) {
    char opp = (myColor == 'R') ? 'B' : 'R';
    int flips = 0;
    for (int d = 0; d < 8; d++) {
        int nx = tx + dx[d], ny = ty + dy[d];
        if (in_bounds(nx, ny) && board[nx][ny] == opp)
            flips++;
    }
    return flips;
}

// Count adjacent my-color cells around (x, y) after move
int count_adj_my_cells(char board[8][9], char myColor, int x, int y) {
    int count = 0;
    for (int d = 0; d < 8; d++) {
        int nx = x + dx[d], ny = y + dy[d];
        if (in_bounds(nx, ny) && board[nx][ny] == myColor)
            count++;
    }
    return count;
}

// Make a copy of the board and apply a move (clone/jump)
void apply_move(char src[8][9], char dest[8][9], char myColor, int sx, int sy, int tx, int ty, int is_clone) {
    memcpy(dest, src, sizeof(char) * 8 * 9);
    if (is_clone) {
        dest[tx][ty] = myColor;
    }
    else {
        dest[sx][sy] = '.';
        dest[tx][ty] = myColor;
    }
    // Flip opponent pieces around destination
    char opp = (myColor == 'R') ? 'B' : 'R';
    for (int d = 0; d < 8; d++) {
        int nx = tx + dx[d], ny = ty + dy[d];
        if (in_bounds(nx, ny) && dest[nx][ny] == opp)
            dest[nx][ny] = myColor;
    }
}

// Check if can move from (sx,sy) to (tx,ty)
int valid_move(char board[8][9], int sx, int sy, int tx, int ty) {
    if (!in_bounds(tx, ty)) return 0;
    if (board[tx][ty] != '.') return 0;
    return 1;
}

// Main function
int* move_generate(char board[8][9], char myColor) {
    static int result[4] = { 0, 0, 0, 0 };
    Move moves[256];
    int move_count = 0;

    // 1. Gather all possible moves
    for (int sx = 0; sx < N; sx++) {
        for (int sy = 0; sy < N; sy++) {
            if (board[sx][sy] != myColor) continue;
            // Clone (adjacent)
            for (int d = 0; d < 8; d++) {
                int tx = sx + dx[d], ty = sy + dy[d];
                if (!valid_move(board, sx, sy, tx, ty)) continue;
                char temp[8][9];
                apply_move(board, temp, myColor, sx, sy, tx, ty, 1);
                int flips = count_flips(board, myColor, tx, ty);
                int gain = flips + 1; // +1 for new clone piece
                int adj = count_adj_my_cells(temp, myColor, tx, ty);
                moves[move_count++] = (Move){ sx, sy, tx, ty, gain, adj, 1 };
            }
            // Jump (2 cells away)
            for (int d = 0; d < 8; d++) {
                int tx = sx + 2 * dx[d], ty = sy + 2 * dy[d];
                if (!valid_move(board, sx, sy, tx, ty)) continue;
                char temp[8][9];
                apply_move(board, temp, myColor, sx, sy, tx, ty, 0);
                int flips = count_flips(board, myColor, tx, ty);
                int gain = flips; // No +1 for jump (the source disappears)
                int adj = count_adj_my_cells(temp, myColor, tx, ty);
                moves[move_count++] = (Move){ sx, sy, tx, ty, gain, adj, 0 };
            }
        }
    }

    // 2. No moves = pass
    if (move_count == 0) {
        result[0] = result[1] = result[2] = result[3] = 0;
        return result;
    }

    // 3. Find max gain
    int max_gain = -1;
    for (int i = 0; i < move_count; i++)
        if (moves[i].gain > max_gain)
            max_gain = moves[i].gain;

    // 4. Collect max gain moves
    Move best_moves[256];
    int best_count = 0;
    for (int i = 0; i < move_count; i++)
        if (moves[i].gain == max_gain)
            best_moves[best_count++] = moves[i];

    // 5. Prefer clones among best moves
    int clone_best = 0;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].is_clone)
            clone_best = 1;
    // If any clone is best, filter only clones
    if (clone_best) {
        int new_count = 0;
        for (int i = 0; i < best_count; i++)
            if (best_moves[i].is_clone)
                best_moves[new_count++] = best_moves[i];
        best_count = new_count;
    }

    // 6. Among best, pick one with most adjacent my-cells after move
    int max_adj = -1;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].adj_my_cells > max_adj)
            max_adj = best_moves[i].adj_my_cells;

    Move final_moves[256];
    int final_count = 0;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].adj_my_cells == max_adj)
            final_moves[final_count++] = best_moves[i];

    // 7. Final tiebreak: lowest coordinates
    Move* chosen = &final_moves[0];
    for (int i = 1; i < final_count; i++) {
        Move* m = &final_moves[i];
        if (m->sx < chosen->sx ||
            (m->sx == chosen->sx && m->sy < chosen->sy) ||
            (m->sx == chosen->sx && m->sy == chosen->sy && m->tx < chosen->tx) ||
            (m->sx == chosen->sx && m->sy == chosen->sy && m->tx == chosen->tx && m->ty < chosen->ty)) {
            chosen = m;
        }
    }

    // 8. Output in 1-based coordinates
    result[0] = chosen->sx + 1;
    result[1] = chosen->sy + 1;
    result[2] = chosen->tx + 1;
    result[3] = chosen->ty + 1;
    return result;
}

void test_function(char board[8][9]) {
    // Print board before move
    printf("Before Move\n");
    for (int i = 0; i < 8; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < 8; j++) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }

    int* mv = move_generate(board, 'R');
    printf("Move: %d %d %d %d\n", mv[0], mv[1], mv[2], mv[3]);

}

void handle_turn(cJSON* json) {
    cJSON* board_arr = cJSON_GetObjectItem(json, "board");

    printf("\n=== Current Board ===\n");
    for (int i = 0; i < SIZE; i++) {
        const char* row = cJSON_GetArrayItem(board_arr, i)->valuestring;
        printf("%s\n", row);
    }

    char board[SIZE][9];  // 안전하게 9로 유지
    for (int i = 0; i < SIZE; i++) {
        const char* row = cJSON_GetArrayItem(board_arr, i)->valuestring;
        strncpy(board[i], row, 8);
        board[i][8] = '\0';  // null-terminate
    }

    char token = (strcmp(username, "Alice") == 0) ? 'R' : 'B';
    int* mv = move_generate(board, token);
    int sx = mv[0], sy = mv[1], tx = mv[2], ty = mv[3];

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

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[ERROR] WSAStartup failed\n"); return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[ERROR] socket() failed\n"); return 1;
    }

    SOCKADDR_IN server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    printf("[DEBUG] Attempting to connect to server...\n"); fflush(stdout);
    if (connect(sock, (SOCKADDR*)&server, sizeof(server)) != 0) {
        printf("[ERROR] Connection failed.\n"); return 1;
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

    closesocket(sock);
    WSACleanup();
    return 0;
}