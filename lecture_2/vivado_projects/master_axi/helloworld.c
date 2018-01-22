/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"

#define N 60000
#define TILE_ELEM 60000
#define TILING_FACTOR (N/TILE_ELEM)
#define my_type float

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include "xnbody.h"
#include "support.h"
#include "xtmrctr.h"
#include "xil_cache.h"
#include "malloc.h"

XNbody n_body_device;

void data_generation(int N_loc, particle_t **particles, float **m){

    *particles = (particle_t *) memalign(64, sizeof(particle_t)*N_loc);
    *m = (float *) memalign(64, sizeof(float)*N_loc);

    int i;

	srand(100);

	for (i = 0; i < N_loc; i++){
            (*m)[i] = (float)rand()/100000;
            (*particles)[i].p.x = (float)rand()/100000;
            (*particles)[i].p.y = (float)rand()/100000;
            (*particles)[i].p.z = (float)rand()/100000;
            (*particles)[i].v.x = (float)rand()/100000;
            (*particles)[i].v.y = (float)rand()/100000;
            (*particles)[i].v.z = (float)rand()/100000;

	}

}

void kernel(XNbody *InstancePtr, my_type *p_x, my_type *p_y, my_type *p_z, my_type *tmp_x, my_type *tmp_y, my_type *tmp_z, my_type *a_x, my_type *a_y, my_type *a_z, my_type EPS, my_type *m){


	XNbody_Set_p_x_V(InstancePtr, (u32)tmp_x);
	XNbody_Set_p_y_V(InstancePtr, (u32)tmp_y);
	XNbody_Set_p_z_V(InstancePtr, (u32)tmp_z);
	XNbody_Set_a_x_V(InstancePtr, (u32)a_x);
	XNbody_Set_a_y_V(InstancePtr, (u32)a_y);
	XNbody_Set_a_z_V(InstancePtr, (u32)a_z);
	XNbody_Set_c_V(InstancePtr, (u32)m);
	XNbody_Set_EPS(InstancePtr, (u32)EPS);
	XNbody_Set_tiling_factor(InstancePtr, (u32)TILING_FACTOR);

	XNbody_Start(InstancePtr);

	while(!XNbody_IsDone(InstancePtr));

}

void run_FPGA(XNbody *InstancePtr, int N_loc, int nt, my_type EPS, my_type *m,
                    const particle_t *in_particles, particle_t *out_particles, u32 *time)
{
    particle_t *p = (particle_t *) memalign(64, sizeof(particle_t)*N_loc);
    memcpy(p, in_particles, N_loc * sizeof(particle_t));

    my_type p_x [N];
    my_type p_y [N];
    my_type p_z [N];

    my_type v_x [N];
    my_type v_y [N];
    my_type v_z [N];

    my_type a_x [N];
    my_type a_y [N];
    my_type a_z [N];

    int i;
    for(i = 0; i < N_loc; i++){
        p_x[i] = p[i].p.x;
    	p_y[i] = p[i].p.y;
        p_z[i] = p[i].p.z;
        v_x[i] = p[i].v.x;
        v_y[i] = p[i].v.y;
        v_z[i] = p[i].v.z;
        a_x[i] = 0;
        a_y[i] = 0;
        a_z[i] = 0;
    }

    int t;

    XTmrCtr timer;

    u32 start, end;


    XTmrCtr_Initialize(&timer, 0);

    XTmrCtr_Reset(&timer, 0);

    for (t = 0; t < nt; t++) {


        printf("Starting compute n body\n\r");

        XTmrCtr_Start(&timer, 0);
        start = XTmrCtr_GetValue(&timer, 0);
        kernel(InstancePtr, p_x, p_y, p_z, p_x, p_y, p_z, a_x, a_y, a_z, EPS, m);
        XTmrCtr_Stop(&timer, 0);
        end = XTmrCtr_GetValue(&timer, 0);
        XTmrCtr_Reset(&timer, 0);

        printf("Updating bodies\n\r");

        int kk;

        for(kk = 0; kk < N_loc; kk++){
        	p_x[kk] += v_x[kk];
        	p_y[kk] += v_y[kk];
        	p_z[kk] += v_z[kk];
        	v_x[kk] += a_x[kk];
        	v_y[kk] += a_y[kk];
        	v_z[kk] += a_z[kk];
        }

        *time += end - start;

    }

    for(i = 0; i < N_loc; i++){
    	p[i].p.x = p_x[i];
    	p[i].p.y = p_y[i];
    	p[i].p.z = p_z[i];
    	p[i].v.x = v_x[i];
    	p[i].v.y = v_y[i];
    	p[i].v.z = v_z[i];
    }

    memcpy(out_particles, p, N_loc * sizeof(particle_t));

    free(p);
}




int initDevice(XNbody *InstancePtr, u16 DeviceId){
	XNbody_Config *CfgPtr;
	int status;
	CfgPtr = XNbody_LookupConfig(DeviceId);
	if (!CfgPtr) {
			xil_printf("Error looking for XCompute_n_body config\n\r");
			return XST_FAILURE;
		}
	status = XNbody_CfgInitialize(InstancePtr, CfgPtr);
		if (status != XST_SUCCESS) {
			xil_printf("Error initializing XCompute_n_body\n\r");
			return XST_FAILURE;
		}

		return XST_SUCCESS;

}


int main(int argc, char **argv)
{
    init_platform();
    Xil_DCacheDisable();
    Xil_ICacheDisable();

    printf("Initialization...");

    int status = initDevice(&n_body_device, 0);

    if(status != XST_SUCCESS){
    	printf("Error in initialization\n");
    }
    printf("Done!\n");

    int N_loc = N;
    int nt = 1;
    float EPS = 100.0;

    if (EPS == 0) {
        fprintf(stderr, "EPS cannot be set to zero\n");
        exit(EXIT_FAILURE);
    }

    particle_t *particles;
    float *m;

    printf("Generating data...");
    data_generation(N_loc, &particles, &m);
    printf("Done!\n");

    u32 fpgaTime = 0;
    particle_t *FPGA_particles = NULL;

    FPGA_particles = (particle_t *) memalign(64, sizeof(particle_t)*N_loc);

    printf("simulating FPGA..\n");
    run_FPGA(&n_body_device, N_loc, nt, EPS, m, particles, FPGA_particles, &fpgaTime);
    printf("FPGA execution time: %lu\n", fpgaTime);

    printf("Results Checked!\r\n");

    free(particles);
    free(m);
    free(FPGA_particles);
    cleanup_platform();

    return 0;
}
