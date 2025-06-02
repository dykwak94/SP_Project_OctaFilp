#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>             // For close()
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cjson/cJSON.h>
#include "board.h"

#define SIZE 8


char username[32];
int sock;   // Linux: socket is an int

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

// used for possible movements
int dx[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
int dy[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

// Check if x, y is in bound of board
int in_bounds(int x, int y) {
    return x >= 0 && x < N && y >= 0 && y < N;
}

// Count number of newly flipped cells if I clone/jump to (tx, ty)
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

// Count adjacent my troop cells around (x, y) (after move, am I surrounded to more same cells?)
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
    // Make a copy so we can simulate the next step without disrupting the original board
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
    if (!in_bounds(tx, ty)) return 0; // in bound check
    if (board[tx][ty] != '.') return 0; // empty check
    return 1;
}

// Main function for our game strategy
/* 
argument: char board[8][9], char myColor (R or B)
output: (sx, sy, tx, ty) in 1-based 

[IDEA of algorithm] Act in an aggressive way in initial states
Concept 1: Maximize the new flipped cells
Concept 2: If clone and jump earn the same new flipped cells, select clone (because it makes our group more robust)
Concept 3: If multiple clones earn the same new-flipped cells, select the one that moves to where more of our troops are located. (i.e. dive into our troops pool for robustness of group)
Concept 4: If stil there are tied cases, just choose the move with lowest coordinates (sx, sy, tx, ty). This move is deterministic and has no particular reason.
*/
int* move_generate(char board[8][9], char myColor) {
    static int result[4] = { 0, 0, 0, 0 };
    Move moves[256];
    int move_count = 0;

    // Gather all possible moves (every clone and jump)
    for (int sx = 0; sx < N; sx++) {
        for (int sy = 0; sy < N; sy++) {
            if (board[sx][sy] != myColor) continue;
            
            // Clone (1-step)
            for (int d = 0; d < 8; d++) {
                int tx = sx + dx[d], ty = sy + dy[d];
                if (!valid_move(board, sx, sy, tx, ty)) continue;
                char temp[8][9];
                apply_move(board, temp, myColor, sx, sy, tx, ty, 1);
                // Compute how many new flips are earned
                int flips = count_flips(board, myColor, tx, ty);
                int gain = flips + 1; // because clone maintain original cell
                int adj = count_adj_my_cells(temp, myColor, tx, ty);
                moves[move_count++] = (Move){ sx, sy, tx, ty, gain, adj, 1 };
            }
            
            // Jump (2-step)
            for (int d = 0; d < 8; d++) {
                int tx = sx + 2 * dx[d], ty = sy + 2 * dy[d];
                if (!valid_move(board, sx, sy, tx, ty)) continue; // handle invalid move
                
                char temp[8][9]; 
                apply_move(board, temp, myColor, sx, sy, tx, ty, 0);
                // Compute how many new flips are earned
                int flips = count_flips(board, myColor, tx, ty);
                int gain = flips; // 
                int adj = count_adj_my_cells(temp, myColor, tx, ty);
                moves[move_count++] = (Move){ sx, sy, tx, ty, gain, adj, 0 };
            }
        }
    }

    // No moves => pass
    if (move_count == 0) {
        result[0] = result[1] = result[2] = result[3] = 0;
        return result;
    }

    // Find argmax_(move) [newly-flipped cells] (i.e. find movement that maximizes the newly-flipped cells)
    int max_gain = -1;
    for (int i = 0; i < move_count; i++)
        if (moves[i].gain > max_gain)
            max_gain = moves[i].gain;

    // Gather possible movements that earns same newly-flipped cells
    Move best_moves[256];
    int best_count = 0;
    for (int i = 0; i < move_count; i++)
        if (moves[i].gain == max_gain)
            best_moves[best_count++] = moves[i];

    // Select clone than jump
    int clone_best = 0;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].is_clone)
            clone_best = 1;
    
    // If one clone is best, filter only clones
    if (clone_best) {
        int new_count = 0;
        for (int i = 0; i < best_count; i++)
            if (best_moves[i].is_clone)
                best_moves[new_count++] = best_moves[i];
        best_count = new_count;
    }

    // Among the best cases, pick one with most adjacent my troop cells after move
    int max_adj = -1;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].adj_my_cells > max_adj)
            max_adj = best_moves[i].adj_my_cells;
    
    Move final_moves[256];
    int final_count = 0;
    for (int i = 0; i < best_count; i++)
        if (best_moves[i].adj_my_cells == max_adj)
            final_moves[final_count++] = best_moves[i];

    // If still first-placed movement is not filtered, choose the move with the lowest coordinates (sx, sy, tx, ty).
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

    // Convert to 1-based coordinates
    result[0] = chosen->sx + 1;
    result[1] = chosen->sy + 1;
    result[2] = chosen->tx + 1;
    result[3] = chosen->ty + 1;
    return result;
}

void print_board(cJSON *board_array) {
    char board[8][9];
    for (int i = 0; i < 8; i++) {
        const char *row = cJSON_GetArrayItem(board_array, i)->valuestring;
        strncpy(board[i], row, 9);
        board[i][8] = '\0'; // null-terminate for safety
        printf("%s\n", board[i]);
    }
    update_led_board(board);  // display on LED panel
    sleep(1);
}

void handle_turn(cJSON* json) {
    cJSON* board_arr = cJSON_GetObjectItem(json, "board");

    printf("\n=== Current Board ===\n");
    print_board(board_arr);

    char board[SIZE][9];  // maintain 9 for safety
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

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket() failed");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    printf("[DEBUG] Attempting to connect to server...\n"); fflush(stdout);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) != 0) {
        perror("[ERROR] Connection failed.");
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

    close(sock); // Linux socket close
    return 0;
}
