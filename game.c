#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>  // Mapeamento de memoria
#include <stdlib.h>
#include <string.h>

#include "maps.h"      // inclui arquivo header

// Cores
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GRAY    0x8410
#define YELLOW  0xFFE0
#define MAGENTA 0xF81F
#define PINK    0xF818
#define CYAN    0x07FF

// Enderecos e valores
#define PERIPHERAL_BASE       0xFF200000
#define PERIPHERAL_SPAN       0x00200000
#define VGA_BASE_PHYSICAL     0xC8000000
#define VGA_SPAN              (512 * 240 * 2)

#define ADDR_OFFSET_HEX0_3    0x20
#define ADDR_OFFSET_BUTTONS   0x50
#define ADDR_OFFSET_TIMER     0x2000 

#define CYCLES_PER_FRAME 833333LL // 50MHz/60 quadros = 833333, isso vai ser tipo um contador para deixar o game em 60 quadros

#define SCREEN_WIDTH      512
#define VISIBLE_WIDTH     320
#define SCREEN_HEIGHT     240
#define GRID_SIZE         10
#define GAME_TIME         60
#define MEMORIZE_TIME     10

#define UP_MASK    0b1000
#define DOWN_MASK  0b0100
#define LEFT_MASK  0b0010
#define RIGHT_MASK 0b0001
#define ANY_BUTTON_MASK 0b1111

#define ADDR_OFFSET_LEDS      0x00 
#define ADDR_OFFSET_SWITCHES  0x40

#define ADDR_OFFSET_AUDIO     0x3040 

// Ponteiros de Hardware e Variáveis Globais
void *peripheral_map_base = NULL;
volatile uint16_t *vga_pixel_buffer = NULL; // Front buffer
uint16_t *back_buffer = NULL;               // Back buffer 
volatile unsigned int *hex0_3_ptr = NULL;
volatile unsigned int *button_ptr = NULL;
volatile unsigned int *timer_base_ptr = NULL;
volatile unsigned int *led_ptr = NULL;
volatile unsigned int *switch_ptr = NULL;
volatile unsigned int *audio_ptr = NULL;

int fd_mem = -1;

typedef struct { int x; int y; } Point;
typedef enum { WELCOME, MEMORIZE, PLAY, GAME_WIN, GAME_LOSE } GameState;

// Controlam as informacoes do game
GameState current_state;
Point player_pos;
Point start_pos = {1, 1};
Point end_pos = {GRID_W - 2, GRID_H - 3};
int time_remaining;
int current_map_index = 0;
int color_player = 0;
uint16_t print_player_color;

// software e hardware
int hardware_init() {
    fd_mem = open("/dev/mem", O_RDWR | O_SYNC);     // Memoria fisica da placa
    if (fd_mem == -1) { perror("Erro ao abrir /dev/mem"); return -1; }
    
    peripheral_map_base = mmap(NULL, PERIPHERAL_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mem, PERIPHERAL_BASE); 
    if (peripheral_map_base == MAP_FAILED) { perror("Erro ao mapear periféricos"); close(fd_mem); return -1; }
    
    vga_pixel_buffer = (uint16_t *)mmap(NULL, VGA_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mem, VGA_BASE_PHYSICAL);
    if (vga_pixel_buffer == MAP_FAILED) { perror("Erro ao mapear VGA"); munmap(peripheral_map_base, PERIPHERAL_SPAN); close(fd_mem); return -1; }

    back_buffer = (uint16_t *)malloc(VISIBLE_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));  // separa RAM para o back_buffer
    if (back_buffer == NULL) { perror("Erro ao alocar back buffer"); return -1; }
    
    // Pega os enderecos atraves de base + offset

    hex0_3_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_HEX0_3);
    button_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_BUTTONS);
    timer_base_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_TIMER);
    led_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_LEDS);
    switch_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_SWITCHES);
    audio_ptr = (volatile unsigned int *)(peripheral_map_base + ADDR_OFFSET_AUDIO); 

    return 0;
}

// limpa as memorias e mapeamentos
void hardware_cleanup() {
    if (back_buffer) free(back_buffer);
    if (vga_pixel_buffer) munmap((void*)vga_pixel_buffer, VGA_SPAN);
    if (peripheral_map_base) munmap(peripheral_map_base, PERIPHERAL_SPAN);
    if (fd_mem != -1) close(fd_mem);
}

// Funcoes uteis

// Desenha um pixel na tela
void set_pix(int y, int x, uint16_t color) {
    if (y >= 0 && y < SCREEN_HEIGHT && x >= 0 && x < VISIBLE_WIDTH) {
        back_buffer[y * VISIBLE_WIDTH + x] = color;
    }
}

// Copia o back buffer e manda para a tela visivel
void present_frame() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        void* source_line = back_buffer + y * VISIBLE_WIDTH;                // Ponteiro para o início da linha Y na origem (back buffer)
        void* dest_line = (void*)(vga_pixel_buffer + y * SCREEN_WIDTH);     // Ponteiro para o início da linha Y no destino (VGA buffer)
        memcpy(dest_line, source_line, VISIBLE_WIDTH * sizeof(uint16_t));   // Copia uma linha inteira (320 pixels)
    }
}

// Desenha tile preenchido (personagem, parede, destino)
void tile(int x_start, int y_start, int x_end, int y_end, uint16_t color) {
    for (int y = y_start; y <= y_end; y++) {
        for (int x = x_start; x <= x_end; x++) {
            set_pix(y, x, color);
        }
    }
}


void draw_circle(int center_x, int center_y, int radius, uint16_t color) {
    // Pré-calcula o raio ao quadrado para otimização
    int r_squared = radius * radius;

    // Percorre um "quadrado" (bounding box) ao redor do círculo
    for (int y = center_y - radius; y <= center_y + radius; y++) {
        for (int x = center_x - radius; x <= center_x + radius; x++) {
            
            // Calcula a distância de cada pixel (x, y) para o centro do círculo
            int dx = x - center_x;
            int dy = y - center_y;

            // Compara o quadrado da distância com o quadrado do raio
            if ((dx * dx) + (dy * dy) <= r_squared) {

                // Se o pixel estiver dentro do círculo entao pinta
                set_pix(y, x, color);
            }
        }
    }
}

// Audio
void play_wall_boop() {
    int duration = 5000;
    int volume = 100000;

    *(audio_ptr) = 0b1100;                  // Limpa o buffer de áudio (FIFO)
    *(audio_ptr) = 0b0000;

    for (int i = 0; i < duration; i++) {
        int space_in_fifo = (*(audio_ptr + 1) & 0xFF000000) >> 24;
        if (space_in_fifo > 20) {
            if (i % 20 > 10) {
                *(audio_ptr + 2) = volume;  // Canal esquerdo
                *(audio_ptr + 3) = volume;  // Canal direito
            } else {
                *(audio_ptr + 2) = -volume;
                *(audio_ptr + 3) = -volume;
            }
        }
    }
}

// Pinta o fundo inteiro com uma cor
void color_background(uint16_t color) {
    tile(0, 0, VISIBLE_WIDTH - 1, SCREEN_HEIGHT - 1, color);
}

// Endereco do {0,1,2,3,4,5,6,7,8,9} no display 7 segmentos
const uint8_t seven_seg_map[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};

void display_7seg(int number) {
    if (number < 0) number = 0; if (number > 99) number = 99;
    *hex0_3_ptr = (seven_seg_map[number / 10] << 8) | seven_seg_map[number % 10];
}

void timer_start(long long clock_cycles) {
    *(timer_base_ptr + 1) = 0b1000;
    *(timer_base_ptr + 2) = clock_cycles & 0xFFFFFFFF;
    *(timer_base_ptr + 3) = 0;
    *(timer_base_ptr + 1) = 0b0111;
}

int timer_has_fired() {
    if (*(timer_base_ptr) & 0x1) { *(timer_base_ptr) = 0x1; return 1; }
    return 0;
}

// valores de inicio
void game_init() {
    player_pos = start_pos;
    current_state = WELCOME;
    display_7seg(0);
}

// Desenha labirinto
void draw_maze(bool is_visible) {
    uint16_t wall_color = is_visible ? WHITE : BLACK;
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (all_mazes[current_map_index][y][x]) {
                tile(x * GRID_SIZE, y * GRID_SIZE, x * GRID_SIZE + GRID_SIZE - 1, y * GRID_SIZE + GRID_SIZE - 1, wall_color);
            }
        }
    }
}

// Desenha jogador
void draw_player() {
    // Vai mudando a cor do personagem
    if(color_player == 7) color_player = 0;

    switch (color_player)
    {
    case 0:
        print_player_color = BLUE;
        break;
    case 1:
        print_player_color = YELLOW;
        break;
    case 2:
        print_player_color = RED;
        break;
    case 3:
        print_player_color = MAGENTA;
        break;
    case 4:
        print_player_color = GRAY;
        break;
    case 5:
        print_player_color = PINK;
        break;
    case 6:
        print_player_color = CYAN;
        break;
    case 7:
        print_player_color = GREEN;
        break;
    
    }
    
    // Checa se o primeiro LED (LEDR0) está aceso.
    // O operador '&' faz uma operação "E" bit a bit.
    // Se o primeiro bit de led_state for 1, o resultado será 1 (verdadeiro).
    
    unsigned int current_led_state = *led_ptr;
    if (current_led_state & 0x1) {

        // LED esta ligado: Desenha um personagem quadrado
        tile(player_pos.x * GRID_SIZE, player_pos.y * GRID_SIZE, 
             player_pos.x * GRID_SIZE + GRID_SIZE - 1, player_pos.y * GRID_SIZE + GRID_SIZE - 1, 
             print_player_color);
    } else {

        // LED esta desligado: Desenha um personagem circular
        int radius = GRID_SIZE / 2;
        int center_x = (player_pos.x * GRID_SIZE) + radius;
        int center_y = (player_pos.y * GRID_SIZE) + radius;
        draw_circle(center_x, center_y, radius, print_player_color);
    }
    color_player++; // Muda a cor do personagem
}

// Desenha Destino 
void draw_destination() {
    tile(end_pos.x * GRID_SIZE, end_pos.y * GRID_SIZE, end_pos.x * GRID_SIZE + GRID_SIZE - 1, end_pos.y * GRID_SIZE + GRID_SIZE - 1, YELLOW);
}

// Logica principal
int main() {
    if (hardware_init() == -1) return 1;
    game_init();
    int frame_counter = 0;
    timer_start(CYCLES_PER_FRAME); 

    while(1) {
        if (timer_has_fired()) {
            unsigned int switch_state = *switch_ptr;

            // 2. Verifica se o primeiro switch (SW0) está para cima (ligado)
            if (switch_state & 0x1) {
                *led_ptr = 0x1; // Liga o primeiro LED (LEDR0)
            } else {
                *led_ptr = 0x0; // Desliga o primeiro LED
            }

            unsigned int buttons_pressed = *button_ptr;

            switch(current_state) {
                // Tela de inicio; ao pressionar botao envia para MEMORIZE
                case WELCOME:
                    color_background(GRAY);
                    display_7seg(88);
                    if (buttons_pressed & ANY_BUTTON_MASK) {
                        current_state = MEMORIZE;
                        time_remaining = MEMORIZE_TIME;
                        display_7seg(time_remaining);
                        frame_counter = 0; 
                    }
                    break;
                
                // Logica de tempo e desenha o mapa
                case MEMORIZE:
                    frame_counter++;
                    if (frame_counter >= 60) {
                        time_remaining--;
                        display_7seg(time_remaining);
                        frame_counter = 0;
                        if (time_remaining <= 0) {
                            current_state = PLAY;
                            time_remaining = GAME_TIME;
                            display_7seg(time_remaining);
                        }
                    }
                    color_background(BLACK);
                    draw_maze(true);
                    draw_player();
                    draw_destination();
                    break;

                // Tempo, botoes, personagem, colisao, audio
                case PLAY:
                    frame_counter++;
                    if (frame_counter >= 60) {
                        time_remaining--;
                        display_7seg(time_remaining);
                        frame_counter = 0;
                        if (time_remaining < 0) {
                            current_state = GAME_LOSE;
                        }
                    }
                    Point next_pos = player_pos;
                    bool moved = false;
                    if (buttons_pressed & UP_MASK)    { next_pos.y--; moved = true; }
                    else if (buttons_pressed & DOWN_MASK)  { next_pos.y++; moved = true; }
                    else if (buttons_pressed & LEFT_MASK)  { next_pos.x--; moved = true; }
                    else if (buttons_pressed & RIGHT_MASK) { next_pos.x++; moved = true; }
                    if (moved) {
                        if (next_pos.x >= 0 && next_pos.x < GRID_W && next_pos.y >= 0 && next_pos.y < GRID_H && !all_mazes[current_map_index][next_pos.y][next_pos.x]) {
                            player_pos = next_pos;
                        }
                        else{
                            play_wall_boop();
                        }
                        if (player_pos.x == end_pos.x && player_pos.y == end_pos.y) {
                            current_state = GAME_WIN;
                        }
                        while(*button_ptr & ANY_BUTTON_MASK);
                    }
                    color_background(BLACK);
                    draw_maze(false);
                    draw_destination();
                    draw_player();
                    break;

                // Se o player chegou ao destino mostra uma tela verde e manda para o proximo mapa
                case GAME_WIN:
                    color_background(GREEN);
                    display_7seg(11);
                    if (buttons_pressed & ANY_BUTTON_MASK){
                        while(*button_ptr & ANY_BUTTON_MASK);
                        current_map_index = (current_map_index + 1) % NUM_MAPS;
                        game_init();
                    }
                    break;

                // Se o player nao chegou a tempo no destino mostra uma tela vermelha e repete o mesmo mapa
                case GAME_LOSE:
                    color_background(RED);
                    display_7seg(0);
                    if (buttons_pressed & ANY_BUTTON_MASK){
                        while(*button_ptr & ANY_BUTTON_MASK);
                        game_init();
                    }
                    break;
            }
            // No final de cada quadro, copia a tela secreta para a tela visível
            present_frame();
        }
    }
    // Limpa as memorias, mapeamentos e encerra o programa
    hardware_cleanup();
    return 0;
}