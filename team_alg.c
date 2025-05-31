#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 8

typedef struct {
    int sx, sy, tx, ty;
    int gain;
    int adj_my_cells;
    int is_clone;
} Move;

int dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
int dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

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
    } else {
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
    static int result[4] = {0, 0, 0, 0};
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
                moves[move_count++] = (Move){sx, sy, tx, ty, gain, adj, 1};
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
                moves[move_count++] = (Move){sx, sy, tx, ty, gain, adj, 0};
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

void test_function(char board[8][9]){
    // Print board before move
    printf("Before Move\n");
    for (int i = 0; i < 8; i++) {
        printf("%d ", i+1);
        for (int j = 0; j < 8; j++) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }

    int* mv = move_generate(board, 'R');
    printf("Move: %d %d %d %d\n", mv[0], mv[1], mv[2], mv[3]); 
    
}

int main() {
    char board1[8][9] = {
        "R......B",
        "........",
        "........",
        "........",
        "........",
        "........",
        "........",
        "B......R"
    };

    char board2[8][9] = {
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRR."
    };

    char board3[8][9] = {
        "..R.....",
        "...B....",
        "........",
        "........",
        "........",
        "........",
        "........",
        "........"
    };

    char board4[8][9] = {
        "..R.....",
        ".RRR....",
        "..B.....",
        "........",
        "........",
        "........",
        "........",
        "........"
    };

    char board5[8][9] = {
        ".R......",
        "RBR.....",
        ".B......",
        "........",
        "........",
        "........",
        "........",
        "........"  
    };  

    char board6[8][9] = {
        "........",
        "..BBB...",
        "..BRB...",
        "..BBB...",
        "........",
        "........",
        "........",
        "........"
    };

    char board7[8][9] = {
        "RRRRRRRR",
        "BBBBBBBB",
        "RRRRRRRR",
        "BBBBBBBB",
        "RRRRRRRR",
        "BBBBBBBB",
        "RRRRRRRR",
        "BBBBBBBB"
    };

    char board8[8][9] = {
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB",
        "BBBBBBBB"
    };

    char board9[8][9] = {
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "RRRRRRRR",
        "........"
    };

    char board10[8][9] = {
        "........",
        "..B.B...",
        ".B.R.B..",
        "..B.B...",
        "........",
        "........",
        "........",
        "........"
    };


    char (*boards[10])[9] = {
        board1, board2, board3, board4, board5, board6, board7, board8, board9, board10
    };

    for (int i = 0; i < 10; ++i) {
        printf("\n=== Test Case %d: ===\n", i + 1);
        test_function(boards[i]);
    }

    return 0;
}

