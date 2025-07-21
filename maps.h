// maps.h

#ifndef MAPS_H
#define MAPS_H

#include <stdbool.h>

// Valores da Grid
#define GRID_W 32
#define GRID_H 24

// Quantidade de Mapas
#define NUM_MAPS 10

// Variavel global dos mapas definida em maps.c
extern const bool all_mazes[NUM_MAPS][GRID_H][GRID_W];

#endif // MAPS_H