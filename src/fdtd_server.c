#include "fdtd_server.h"

void* fdtd_solver(void *arg_data){
    fdtd_data_t *data = (fdtd_data_t *)arg_data;
    int sx = data->energy_pixel[0]; //to have easy access to the cell
    int sy = data->energy_pixel[1];

    for(int i = 0; i < data->steps; i++){
        //Main Loop

        //Magentic field changes
        for(int m = 0; m < data->nx_ny[0]; m++){
            for(int n = 0; n < data->nx_ny[1]; n++){
                data->Hx[m][n] -= (SC/IMPENDENT_VOID) * (data->Ez[m][n+1] - data->Ez[m][n]); 
            }
        }
    }

}