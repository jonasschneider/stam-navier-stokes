#include "solver.h"

#include "file2string.h"

int SOLVE_STEPS = 2;

void addSource(int NW, int NH, float* x, float* source, float dt) {
	int size = (NW + 2) * (NH + 2);
	for (int i = 0 ; i < size ; i++) {
		x[i] += dt * source[i];
	}
}

void setBoundary(int NW, int NH, int b, float* x) {
	// can be optimized into 1 for loop if we are using a square grid
	for (int i = 1 ; i <= NH; i++) {
		// along the left wall
		x[IX(0,    i)]        = b == 1 ? -x[IX(1, i)]  : x[IX(1, i)];
		
		// along the right wall
		x[IX(NW + 1, i)]      = b == 1 ? -x[IX(NW, i)] : x[IX(NW, i)];
 	}
 
 	for (int i = 1 ; i <= NW; i++) {
		// along the top wall
		x[IX(i,    0)]      = b == 2 ? -x[IX(i, 1)]  : x[IX(i, 1)];
		
		// along the bottom wall
		x[IX(i,    NH + 1)] = b == 2 ? -x[IX(i, NH)] : x[IX(i, NH)];
	}
  
	x[IX(0,           0)] = 0.5f * (x[IX(1,  0 )]     + x[IX(0,     1)]);
	x[IX(0,      NH + 1)] = 0.5f * (x[IX(1,  NH + 1)] + x[IX(0,     NH)]);
	x[IX(NW + 1,      0)] = 0.5f * (x[IX(NW, 0)]      + x[IX(NW + 1, 1)]);
	x[IX(NW + 1, NH + 1)] = 0.5f * (x[IX(NW, NH + 1)] + x[IX(NW + 1, NH)]);
}

void linearSolve(int NW, int NH, int b, float* x, float* x0, float a, float c) {
	for (int solveIteration = 0 ; solveIteration < 2; solveIteration++) {
    for (int i = 1 ; i <= NW; i++) {
      for (int j = 1 ; j <= NH; j++) {
        x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] + x[IX(i, j - 1)] + x[IX(i, j + 1)])) / c;
      }
    }
		setBoundary(NW, NH, b, x);
	}
}

void diffuse(int NW, int NH, int b, float* x, float* x0, float diff, float dt) {
	float a = dt * diff * NW * NH;
	linearSolve(NW, NH, b, x, x0, a, 1 + 4 * a);
}

void advect(int NW, int NH, int b, float* d, float* d0, float* u, float* v, float dt) {
	float dt0 = dt * NW;

  for (int i = 1; i <= NW; i++) {
    for (int j = 1; j <= NH; j++) {
      float x = i - dt0 * u[IX(i,j)];
      float y = j - dt0 * v[IX(i,j)];
      
      if (x < 0.5f) {
        x = 0.5f;
      }
      
      if (x > NW + 0.5f) {
        x = NW + 0.5f;
      }
      
      int i0 = (int)x;
      int i1 = i0 + 1;
      
      if (y < 0.5f) {
        y = 0.5f;
      }
      
      if (y > NH + 0.5f) {
        y = NH + 0.5f;
      }
      
      int j0 = (int)y;
      int j1 = j0 + 1;
      
      float s1 = x - i0;
      float s0 = 1 - s1;
      float t1 = y - j0;
      float t0 = 1 - t1;
      
      d[IX(i,j)] = s0 * (t0 * d0[IX(i0,j0)] + t1 * d0[IX(i0,j1)]) + s1 * (t0 * d0[IX(i1,j0)] + t1 * d0[IX(i1,j1)]);
    }
  }
	setBoundary(NW, NH, b, d);
}

void project(int NW, int NH, float* u, float* v, float* p, float* div) {
  for (int i = 1 ; i <= NW; i++) {
    for (int j = 1 ; j <= NH; j++) {
      div[IX(i,j)] = -0.5f * (u[IX(i + 1, j)] - u[IX(i - 1, j)] + v[IX(i, j + 1)]-v[IX(i, j - 1)]) / NW;
      p[IX(i, j)] = 0;
    }
  }
  
	setBoundary(NW, NH, 0, div);
  setBoundary(NW, NH, 0, p);
  
	linearSolve(NW, NH, 0, p, div, 1, 4);
  
  for (int i = 1 ; i <= NW; i++) {
    for (int j = 1 ; j <= NH; j++) {
      u[IX(i,j)] -= 0.5f * NW * (p[IX(i + 1, j)] - p[IX(i - 1,j)]);
      v[IX(i,j)] -= 0.5f * NH * (p[IX(i, j + 1)] - p[IX(i, j - 1)]);
    }
  }
  setBoundary(NW, NH, 1, u); 
	setBoundary(NW, NH, 2, v);
}

void stepDensity(int NW, int NH, float* u, float* v, float* u_prev, float* v_prev, float diff, float dt, unsigned int bufferSize) {
  
  printBuffer(u, NW, bufferSize);
	addSource(NW, NH, u, v, dt);
  
	//SWAP(v, u);
  diffuse(NW, NH, 0, v, u, diff, dt);

  printBuffer(u, NW, bufferSize);
	//SWAP(v, u);
  advect(NW, NH, 0, u, v, u_prev, v_prev, dt);
}

void stepVelocity(int NW, int NH, float* u, float* v, float* u_prev, float* v_prev, float visc, float dt) {
	addSource(NW, NH, u, u_prev, dt);
  addSource(NW, NH, v, v_prev, dt);
  
	SWAP(u_prev, u);
  diffuse(NW, NH, 1, u, u_prev, visc, dt);
	
  SWAP(v_prev, v);
  diffuse(NW, NH, 2, v, v_prev, visc, dt);
	
  project(NW, NH, u, v, u_prev, v_prev);
	SWAP(u_prev, u);
  SWAP(v_prev, v);
  
	advect(NW, NH, 1, u, u_prev, u_prev, v_prev, dt);
	advect(NW, NH, 2, v, v_prev, u_prev, v_prev, dt);

	project(NW, NH, u, v, u_prev, v_prev);
}

