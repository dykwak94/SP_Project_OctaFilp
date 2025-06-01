#define _CRT_SECURE_NO_WARNINGS //VS에서 발생하는 overflow 무시하기 위함
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 8

char board[SIZE][SIZE];
char currentplayer = 'R';
int passstack = 0;

int dr[8] = { -1,-1,-1,0,1,1,1,0 };
int dc[8] = { -1,0,1,1,1,0,-1,-1 };

int boardvalid(char* line) {
    if (strlen(line) != 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (line[i] != 'R' && line[i] != 'B' && line[i] != '.' && line[i] != '#')
            return 0;
    }
    return 1;
}

int inboard(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}

char changeplayer(char p) {
    return p == 'R' ? 'B' : 'R';
}

void flip(int r, int c) {
    char opp = changeplayer(currentplayer);
    for (int d = 0; d < 8; d++) {
        int nr = r + dr[d], nc = c + dc[d];
        if (inboard(nr, nc) && board[nr][nc] == opp)
            board[nr][nc] = currentplayer;
    }
}

int findspace(char player) {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (board[r][c] != player) continue;
            for (int d = 0; d < 8; ++d) {
                int nr1 = r + dr[d], nc1 = c + dc[d];
                int nr2 = r + 2 * dr[d], nc2 = c + 2 * dc[d];
                if (inboard(nr1, nc1) && board[nr1][nc1] == '.') return 1;
                if (inboard(nr2, nc2) && board[nr2][nc2] == '.') return 1;
            }
        }
    }
    return 0;
}

int fullboard() {
    for (int i = 0; i < SIZE; ++i)
        for (int j = 0; j < SIZE; ++j)
            if (board[i][j] == '.')
                return 0;
    return 1;
}

void tokennum(int* r, int* b) {
    *r = *b = 0;
    for (int i = 0; i < SIZE; ++i)
        for (int j = 0; j < SIZE; ++j) {
            if (board[i][j] == 'R') (*r)++;
            else if (board[i][j] == 'B') (*b)++;
        }
}

void prt_board_winner() {
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j)
            putchar(board[i][j]);
        putchar('\n');
    }

    int r_cnt, b_cnt;
    tokennum(&r_cnt, &b_cnt);
    if (r_cnt > b_cnt) printf("Red\n");
    else if (b_cnt > r_cnt) printf("Blue\n");
    else printf("Draw\n");
}

int excutemove(int r1, int c1, int r2, int c2) {
    if (r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0) {
        if (!findspace(currentplayer)) {
            passstack++;
            currentplayer = changeplayer(currentplayer);
            return 0;
        }
        else {
            return -1;
        }
    }

    r1--; c1--; r2--; c2--;

    if (!inboard(r1, c1) || !inboard(r2, c2)) return -1;
    if (board[r1][c1] != currentplayer) return -1;
    if (board[r2][c2] != '.') return -1;

    for (int d = 0; d < 8; d++) {
        int nr = r1 + dr[d], nc = c1 + dc[d];
        int jr = r1 + 2 * dr[d], jc = c1 + 2 * dc[d];
        if (r2 == nr && c2 == nc) {
            board[r2][c2] = currentplayer;
            flip(r2, c2);
            passstack = 0;
            currentplayer = changeplayer(currentplayer);
            return 0;
        }
        else if (r2 == jr && c2 == jc) {
            board[r2][c2] = currentplayer;
            board[r1][c1] = '.';
            flip(r2, c2);
            passstack = 0;
            currentplayer = changeplayer(currentplayer);
            return 0;
        }
    }

    return -1;
}

int main() {
    for (int i = 0; i < SIZE; ++i) {
        char line[16];
        if (!fgets(line, sizeof(line), stdin)) {
            printf("Board input error\n");
            return 0;
        }
        line[strcspn(line, "\n")] = '\0';
        if (!boardvalid(line)) {
            printf("Board input error\n");
            return 0;
        }
        for (int j = 0; j < SIZE; ++j)
            board[i][j] = line[j];
    }

    int N;
    if (scanf("%d", &N) != 1 || N < 0) {
        printf("Invalid input at turn 0\n");
        return 0;
    }

    for (int turn = 1; turn <= N; ++turn) {
        int r1, c1, r2, c2;
        if (scanf("%d %d %d %d", &r1, &c1, &r2, &c2) != 4 || r1 < 0 || c1 < 0 || r2 < 0 || c2 < 0) {
            printf("Invalid input at turn %d\n", turn);
            return 0;
        }

        if (excutemove(r1, c1, r2, c2) != 0) {
            printf("Invalid move at turn %d\n", turn);
            return 0;
        }

        int r_cnt, b_cnt;
        tokennum(&r_cnt, &b_cnt);
        if (r_cnt == 0 || b_cnt == 0 || passstack == 2 || fullboard()) {
            prt_board_winner();
            return 0;
        }
    }

    prt_board_winner();
    return 0;
}