#ifndef FDTD_SERVER
#define FDTD_SERVER

#define SC (1/sqrt(2))
#define IMPENDENT_VOID 376,730313668 
#define RELATIVITY 1
#define PPW 20

#include <math.h>
#include <stdio.h>



typedef struct { //struct should be made beffore entering buffer
    int nx_ny [2]; //size of the energy array
    int **Ez; //multidimensional arrays of electric and magnetic fields, shoudl be initialized on 0s
    int **Hx;
    int **Hy;
    int steps; //nuember of steps to be perform
    int k_step; //number of steps each time data goes to the instants buffer 
    int energy_pixel[2]; //direction of the cell that represent the energy added to the electric field, sx and sy
} fdtd_data_t;

void* ftdt_solver(void *arg_data);

#endif