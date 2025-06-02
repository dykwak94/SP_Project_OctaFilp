#include "led-matrix-c.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define MATRIX_SIZE 64
#define BOARD_SIZE 8
#define CELL_SIZE (MATRIX_SIZE / BOARD_SIZE)

char board[BOARD_SIZE][BOARD_SIZE+1];

int boardvalid(char* line) {
    if (strlen(line) != 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (line[i] != 'R' && line[i] != 'B' && line[i] != '.' && line[i] != '#')
            return 0;
    }
    return 1;
}

Color get_color(char c) {
    switch(c) {
        case 'R': return (Color){255, 0, 0};    // Red
        case 'B': return (Color){0, 0, 255};    // Blue
        case '#': return (Color){0, 255, 0};    // Green (obstacle)
        default:  return (Color){0, 0, 0};      // Black (empty)
    }
}

// 전역으로 매트릭스 핸들 유지 (처음 한 번만 생성)
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;

void update_led_board(char board[8][9]) {
    // 최초 1회만 매트릭스 초기화
    if (!matrix) {
        struct RGBLedMatrixOptions options;
        struct RGBLedRuntimeOptions rt_options;
        memset(&options, 0, sizeof(options));
        memset(&rt_options, 0, sizeof(rt_options));
        options.rows = 64;
        options.cols = 64;
        options.chain_length = 1;
        options.parallel = 1;
        options.hardware_mapping = "regular";
        matrix = led_matrix_create_from_options_and_rt_options(&options, &rt_options);
        if (!matrix) {
            fprintf(stderr, "Could not initialize LED matrix.\n");
            return;
        }
        canvas = led_matrix_get_canvas(matrix);
        led_matrix_set_brightness(matrix, 50);
    }

    led_canvas_clear(canvas);

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            Color c = get_color(board[row][col]);
            if (board[row][col] == '.') continue;
            int start_x = col * CELL_SIZE + 1;
            int start_y = row * CELL_SIZE + 1;
            for (int y = start_y; y < start_y + 6; y++) {
                for (int x = start_x; x < start_x + 6; x++) {
                    led_canvas_set_pixel(canvas, x, y, c.r, c.g, c.b);
                }
            }
        }
    }
    // 화면 갱신 필요시 추가 코드 (라이브러리 따라 다름)
}
#ifdef BOARD_MAIN
int main() {
    for (int i = 0; i < BOARD_SIZE; ++i) {
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
        for (int j = 0; j < BOARD_SIZE; ++j){
            board[i][j] = line[j];
        }
    }
    update_led_board(board);
    sleep(10);
    return 0;
}
#endif
