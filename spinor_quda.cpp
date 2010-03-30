#include <stdlib.h>
#include <stdio.h>

#include <quda.h>
#include <spinor_quda.h>
#include <util_quda.h>

#include <xmmintrin.h>

// GPU clover matrix
FullClover cudaClover;

// Pinned memory for cpu-gpu memory copying
void *packedSpinor1 = 0;
void *packedSpinor2 = 0;

int L[4];

ParitySpinor allocateParitySpinor(int *X, Precision precision) {
  ParitySpinor ret;

  ret.precision = precision;
  ret.volume = 1;
  for (int d=0; d<4; d++) {
    ret.X[d] = X[d];
    ret.volume *= X[d];
    L[d] = X[d];
  }
  //ret.volume = volume/2;
  ret.Nc = 3;
  ret.Ns = 1;
  ret.length = ret.volume*ret.Nc*ret.Ns*2;

  if (precision == QUDA_DOUBLE_PRECISION) ret.bytes = ret.length*sizeof(double);
  else if (precision == QUDA_SINGLE_PRECISION) ret.bytes = ret.length*sizeof(float);
  else ret.bytes = ret.length*sizeof(short);

  if (cudaMalloc((void**)&ret.spinor, ret.bytes) == cudaErrorMemoryAllocation) {
    printf("Error allocating spinor\n");
    exit(0);
  }
  
  cudaMemset(ret.spinor, 0, ret.bytes);
  
  if (precision == QUDA_HALF_PRECISION) {
      if (cudaMalloc((void**)&ret.spinorNorm, 2*ret.bytes/spinorSiteSize) == cudaErrorMemoryAllocation) {
      printf("Error allocating spinorNorm\n");
      exit(0);
    }
  }

  return ret;
}


FullSpinor allocateSpinorField(int *X, Precision precision) {
  FullSpinor ret;
  ret.even = allocateParitySpinor(X, precision);
  ret.odd = allocateParitySpinor(X, precision);
  return ret;
}

ParityClover allocateParityClover(int volume, Precision precision) {
  ParityClover ret;

  ret.precision = precision;
  ret.volume = volume;
  ret.Nc = 3;
  ret.Ns = 4;
  ret.length = ret.volume*ret.Nc*ret.Nc*ret.Ns*ret.Ns*2;

  if (precision == QUDA_DOUBLE_PRECISION) ret.bytes = ret.length*sizeof(double);
  else if (precision == QUDA_SINGLE_PRECISION) ret.bytes = ret.length*sizeof(float);
  else ret.bytes = ret.length*sizeof(short);

  if (cudaMalloc((void**)&(ret.clover), ret.bytes) == cudaErrorMemoryAllocation) {
    printf("Error allocating clover term\n");
    exit(0);
  }   

  if (cudaMalloc((void**)&(ret.cloverInverse), ret.bytes) == cudaErrorMemoryAllocation) {
    printf("Error allocating clover term\n");
    exit(0);
  }   

  return ret;
}


FullClover allocateCloverField(int V, Precision precision) {
  FullClover ret;
  ret.even = allocateParityClover(V/2, precision);
  ret.odd = allocateParityClover(V/2, precision);
  return ret;
}

void freeParitySpinor(ParitySpinor spinor) {

  cudaFree(spinor.spinor);
  if (spinor.precision == QUDA_HALF_PRECISION) cudaFree(spinor.spinorNorm);

  spinor.spinor = NULL;
  spinor.spinorNorm = NULL;
}

void freeParityClover(ParityClover clover) {
  cudaFree(clover.clover);
  cudaFree(clover.cloverInverse);
}

void freeSpinorField(FullSpinor spinor) {
  freeParitySpinor(spinor.even);
  freeParitySpinor(spinor.odd);
}

void freeCloverField(FullClover clover) {
  freeParityClover(clover.even);
  freeParityClover(clover.odd);
}

void freeSpinorBuffer() {
#ifndef __DEVICE_EMULATION__
  cudaFreeHost(packedSpinor1);
#else
  free(packedSpinor1);
#endif
  packedSpinor1 = NULL;
}

template <typename Float>
inline void packSpinorVector(float4* a, Float *b, int V) {
  Float K = 1.0 / 2.0;

  a[0*V].x = K*(b[1*6+0*2+0]+b[3*6+0*2+0]);
  a[0*V].y = K*(b[1*6+0*2+1]+b[3*6+0*2+1]);
  a[0*V].z = K*(b[1*6+1*2+0]+b[3*6+1*2+0]);
  a[0*V].w = K*(b[1*6+1*2+1]+b[3*6+1*2+1]);
  
  a[1*V].x = K*(b[1*6+2*2+0]+b[3*6+2*2+0]);
  a[1*V].y = K*(b[1*6+2*2+1]+b[3*6+2*2+1]);
  a[1*V].z = -K*(b[2*6+0*2+0]+b[0*6+0*2+0]);
  a[1*V].w = -K*(b[2*6+0*2+1]+b[0*6+0*2+1]);
  
  a[2*V].x = -K*(b[0*6+1*2+0]+b[2*6+1*2+0]);
  a[2*V].y = -K*(b[0*6+1*2+1]+b[2*6+1*2+1]);
  a[2*V].z = -K*(b[0*6+2*2+0]+b[2*6+2*2+0]);
  a[2*V].w = -K*(b[0*6+2*2+1]+b[2*6+2*2+1]);

  a[3*V].x = K*(b[1*6+0*2+0]-b[3*6+0*2+0]);
  a[3*V].y = K*(b[1*6+0*2+1]-b[3*6+0*2+1]);
  a[3*V].z = K*(b[1*6+1*2+0]-b[3*6+1*2+0]);
  a[3*V].w = K*(b[1*6+1*2+1]-b[3*6+1*2+1]);

  a[4*V].x = K*(b[1*6+2*2+0]-b[3*6+2*2+0]);
  a[4*V].y = K*(b[1*6+2*2+1]-b[3*6+2*2+1]);
  a[4*V].z = K*(b[2*6+0*2+0]-b[0*6+0*2+0]);
  a[4*V].w = K*(b[2*6+0*2+1]-b[0*6+0*2+1]);

  a[5*V].x = K*(b[2*6+1*2+0]-b[0*6+1*2+0]);
  a[5*V].y = K*(b[2*6+1*2+1]-b[0*6+1*2+1]);
  a[5*V].z = K*(b[2*6+2*2+0]-b[0*6+2*2+0]);
  a[5*V].w = K*(b[2*6+2*2+1]-b[0*6+2*2+1]);
}

template <typename Float>
inline void packSpinorVector(float2* a, Float *b, int V) 
{    
    a[0*V].x = (float)b[0];
    a[0*V].y = (float)b[1];
    
    a[1*V].x = (float)b[2];
    a[1*V].y = (float)b[3];
    
    a[2*V].x = (float)b[4];
    a[2*V].y = (float)b[5];
    
}


template <typename Float>
inline void packQDPSpinorVector(float4* a, Float *b, int V) {
  Float K = 1.0 / 2.0;

  a[0*V].x = K*(b[(0*4+1)*2+0]+b[(0*4+3)*2+0]);
  a[0*V].y = K*(b[(0*4+1)*2+1]+b[(0*4+3)*2+1]);
  a[0*V].z = K*(b[(1*4+1)*2+0]+b[(1*4+3)*2+0]);
  a[0*V].w = K*(b[(1*4+1)*2+1]+b[(1*4+3)*2+1]);

  a[1*V].x = K*(b[(2*4+1)*2+0]+b[(2*4+3)*2+0]);
  a[1*V].y = K*(b[(2*4+1)*2+1]+b[(2*4+3)*2+1]);
  a[1*V].z = -K*(b[(0*4+0)*2+0]+b[(0*4+2)*2+0]);
  a[1*V].w = -K*(b[(0*4+0)*2+1]+b[(0*4+2)*2+1]);

  a[2*V].x = -K*(b[(1*4+0)*2+0]+b[(1*4+2)*2+0]);
  a[2*V].y = -K*(b[(1*4+0)*2+1]+b[(1*4+2)*2+1]);
  a[2*V].z = -K*(b[(2*4+0)*2+0]+b[(2*4+2)*2+0]);
  a[2*V].w = -K*(b[(2*4+0)*2+1]+b[(2*4+2)*2+1]);

  a[3*V].x = K*(b[(0*4+1)*2+0]+b[(0*4+3)*2+0]);
  a[3*V].y = K*(b[(0*4+1)*2+1]+b[(0*4+3)*2+1]);
  a[3*V].z = K*(b[(1*4+1)*2+0]+b[(1*4+3)*2+0]);
  a[3*V].w = K*(b[(1*4+1)*2+1]+b[(1*4+3)*2+1]);

  a[4*V].x = K*(b[(2*4+1)*2+0]+b[(2*4+3)*2+0]);
  a[4*V].y = K*(b[(2*4+1)*2+1]+b[(2*4+3)*2+1]);
  a[4*V].z = K*(b[(0*4+2)*2+0]+b[(0*4+0)*2+0]);
  a[4*V].w = K*(b[(0*4+2)*2+1]+b[(0*4+0)*2+1]);

  a[5*V].x = K*(b[(1*4+2)*2+0]+b[(1*4+0)*2+0]);
  a[5*V].y = K*(b[(1*4+2)*2+1]+b[(1*4+0)*2+1]);
  a[5*V].z = K*(b[(2*4+2)*2+0]+b[(2*4+0)*2+0]);
  a[5*V].w = K*(b[(2*4+2)*2+1]+b[(2*4+0)*2+1]);
}

template <typename Float>
inline void packSpinorVector(double2* a, Float *b, int V) 
{  
    a[0*V].x = (double)b[0];
    a[0*V].y = (double)b[1];
    
    a[1*V].x = (double)b[2];
    a[1*V].y = (double)b[3];
    
    a[2*V].x = (double)b[4];
    a[2*V].y = (double)b[5];    

}

template <typename Float>
inline void packQDPSpinorVector(double2* a, Float *b, int V) {
  Float K = 1.0 / 2.0;

  for (int c=0; c<3; c++) {
    a[c*V].x = K*(b[(c*4+1)*2+0]+b[(c*4+3)*2+0]);
    a[c*V].y = K*(b[(c*4+1)*2+1]+b[(c*4+3)*2+1]);

    a[(3+c)*V].x = -K*(b[(c*4+0)*2+0]+b[(c*4+2)*2+0]);
    a[(3+c)*V].y = -K*(b[(c*4+0)*2+1]+b[(c*4+2)*2+1]);

    a[(6+c)*V].x = K*(b[(c*4+1)*2+0]-b[(c*4+3)*2+0]);
    a[(6+c)*V].y = K*(b[(c*4+1)*2+1]-b[(c*4+3)*2+1]);

    a[(9+c)*V].x = K*(b[(c*4+2)*2+0]-b[(c*4+0)*2+0]);
    a[(9+c)*V].y = K*(b[(c*4+2)*2+1]-b[(c*4+0)*2+1]);
  }

}

template <typename Float>
inline void unpackSpinorVector(Float *a, float4 *b, int V) {
  Float K = 1.0;
  a[0*6+0*2+0] = -K*(b[V].z+b[4*V].z);
  a[0*6+0*2+1] = -K*(b[V].w+b[4*V].w);
  a[0*6+1*2+0] = -K*(b[2*V].x+b[5*V].x);
  a[0*6+1*2+1] = -K*(b[2*V].y+b[5*V].y);
  a[0*6+2*2+0] = -K*(b[2*V].z+b[5*V].z);
  a[0*6+2*2+1] = -K*(b[2*V].w+b[5*V].w);
  
  a[1*6+0*2+0] = K*(b[0].x+b[3*V].x);
  a[1*6+0*2+1] = K*(b[0].y+b[3*V].y);
  a[1*6+1*2+0] = K*(b[0].z+b[3*V].z);
  a[1*6+1*2+1] = K*(b[0].w+b[3*V].w);  
  a[1*6+2*2+0] = K*(b[V].x+b[4*V].x);
  a[1*6+2*2+1] = K*(b[V].y+b[4*V].y);
  
  a[2*6+0*2+0] = -K*(b[V].z-b[4*V].z);
  a[2*6+0*2+1] = -K*(b[V].w-b[4*V].w);
  a[2*6+1*2+0] = -K*(b[2*V].x-b[5*V].x);
  a[2*6+1*2+1] = -K*(b[2*V].y-b[5*V].y);
  a[2*6+2*2+0] = -K*(b[2*V].z-b[5*V].z);
  a[2*6+2*2+1] = -K*(b[2*V].w-b[5*V].w);
  
  a[3*6+0*2+0] = -K*(b[3*V].x-b[0].x);
  a[3*6+0*2+1] = -K*(b[3*V].y-b[0].y);
  a[3*6+1*2+0] = -K*(b[3*V].z-b[0].z);
  a[3*6+1*2+1] = -K*(b[3*V].w-b[0].w);
  a[3*6+2*2+0] = -K*(b[4*V].x-b[V].x);
  a[3*6+2*2+1] = -K*(b[4*V].y-b[V].y);
}
template <typename Float>
inline void unpackSpinorVector(Float *a, float2 *b, int V) 
{
    
    a[0] = (Float)b[0*V].x;
    a[1] = (Float)b[0*V].y;
    
    a[2] = (Float)b[1*V].x;
    a[3] = (Float)b[1*V].y;

    a[4] = (Float)b[2*V].x;
    a[5] = (Float)b[2*V].y;
    
}
template <typename Float>
inline void unpackQDPSpinorVector(Float *a, float4 *b, int V) {
  Float K = 1.0;

  a[(0*4+0)*2+0] = -K*(b[V].z+b[4*V].z);
  a[(0*4+0)*2+1] = -K*(b[V].w+b[4*V].w);
  a[(1*4+0)*2+0] = -K*(b[2*V].x+b[5*V].x);
  a[(1*4+0)*2+1] = -K*(b[2*V].y+b[5*V].y);
  a[(2*4+0)*2+0] = -K*(b[2*V].z+b[5*V].z);
  a[(2*4+0)*2+1] = -K*(b[2*V].w+b[5*V].w);
  
  a[(0*4+1)*2+0] = K*(b[0].x+b[3*V].x);
  a[(0*4+1)*2+1] = K*(b[0].y+b[3*V].y);
  a[(1*4+1)*2+0] = K*(b[0].z+b[3*V].z);
  a[(1*4+1)*2+1] = K*(b[0].w+b[3*V].w);  
  a[(2*4+1)*2+0] = K*(b[V].x+b[4*V].x);
  a[(2*4+1)*2+1] = K*(b[V].y+b[4*V].y);
  
  a[(0*4+2)*2+0] = -K*(b[V].z-b[4*V].z);
  a[(0*4+2)*2+1] = -K*(b[V].w-b[4*V].w);
  a[(1*4+2)*2+0] = -K*(b[2*V].x-b[5*V].x);
  a[(1*4+2)*2+1] = -K*(b[2*V].y-b[5*V].y);
  a[(2*4+2)*2+0] = -K*(b[2*V].z-b[5*V].z);
  a[(2*4+2)*2+1] = -K*(b[2*V].w-b[5*V].w);
  
  a[(0*4+3)*2+0] = -K*(b[3*V].x-b[0].x);
  a[(0*4+3)*2+1] = -K*(b[3*V].y-b[0].y);
  a[(1*4+3)*2+0] = -K*(b[3*V].z-b[0].z);
  a[(1*4+3)*2+1] = -K*(b[3*V].w-b[0].w);
  a[(2*4+3)*2+0] = -K*(b[4*V].x-b[V].x);
  a[(2*4+3)*2+1] = -K*(b[4*V].y-b[V].y);
}

template <typename Float>
inline void unpackSpinorVector(Float *a, double2 *b, int V) 
{
    
    a[0] = (Float)b[0*V].x;
    a[1] = (Float)b[0*V].y;
    
    a[2] = (Float)b[1*V].x;
    a[3] = (Float)b[1*V].y;
    
    a[4] = (Float)b[2*V].x;
    a[5] = (Float)b[2*V].y;
}

template <typename Float>
inline void unpackQDPSpinorVector(Float *a, double2 *b, int V) {
  Float K = 1.0;

  for (int c=0; c<3; c++) {
    a[(c*4+0)*2+0] = -K*(b[(3+c)*V].x+b[(9+c)*V].x);
    a[(c*4+0)*2+1] = -K*(b[(3+c)*V].y+b[(9+c)*V].y);

    a[(c*4+1)*2+0] = K*(b[c*V].x+b[(6+c)*V].x);
    a[(c*4+1)*2+1] = K*(b[c*V].y+b[(6+c)*V].y);

    a[(c*4+2)*2+0] = -K*(b[(3+c)*V].x-b[(9+c)*V].x);
    a[(c*4+2)*2+1] = -K*(b[(3+c)*V].y-b[(9+c)*V].y);
    
    a[(c*4+3)*2+0] = -K*(b[(6+c)*V].x-b[c*V].x);
    a[(c*4+3)*2+1] = -K*(b[(6+c)*V].y-b[c*V].y);
  }

}

// Standard spinor packing, colour inside spin
template <typename Float, typename FloatN>
void packParitySpinor(FloatN *res, Float *spinor, int Vh) 
{
    for (int i = 0; i < Vh; i++) {
	packSpinorVector(res+i, spinor+spinorSiteSize*i, Vh);
    }
}

template <typename Float, typename FloatN>
void unpackParitySpinor(Float *res, FloatN *spinorPacked, int Vh) {

  for (int i = 0; i < Vh; i++) {
      unpackSpinorVector(res+i*spinorSiteSize, spinorPacked+i, Vh);
  }
}

// QDP spinor packing, spin inside colour
template <typename Float, typename FloatN>
void packQDPParitySpinor(FloatN *res, Float *spinor, int Vh) {
  for (int i = 0; i < Vh; i++) {
    packQDPSpinorVector(res+i, spinor+i*24, Vh);
  }
}

// QDP spinor packing, spin inside colour
template <typename Float, typename FloatN>
void unpackQDPParitySpinor(Float *res, FloatN *spinor, int Vh) {
  for (int i = 0; i < Vh; i++) {
    unpackQDPSpinorVector(res+i*24, spinor+i, Vh);
  }
}

template <typename Float, typename FloatN>
void packFullSpinor(FloatN *even, FloatN *odd, Float *spinor, int Vh) {

  for (int i=0; i<Vh; i++) {

    int boundaryCrossings = i/L[0] + i/(L[1]*L[0]) + i/(L[2]*L[1]*L[0]);

    { // even sites
      int k = 2*i + boundaryCrossings%2; 
      packSpinorVector(even+i, spinor+24*k, Vh);
    }
    
    { // odd sites
      int k = 2*i + (boundaryCrossings+1)%2;
      packSpinorVector(odd+i, spinor+24*k, Vh);
    }
  }
}

template <typename Float, typename FloatN>
void unpackFullSpinor(Float *res, FloatN *even, FloatN *odd, int Vh) {

  for (int i=0; i<Vh; i++) {

    int boundaryCrossings = i/L[0] + i/(L[1]*L[0]) + i/(L[2]*L[1]*L[0]);

    { // even sites
      int k = 2*i + boundaryCrossings%2; 
      unpackSpinorVector(res+24*k, even+i, Vh);
    }
    
    { // odd sites
      int k = 2*i + (boundaryCrossings+1)%2;
      unpackSpinorVector(res+24*k, odd+i, Vh);
    }
  }
}

void
loadParitySpinor(ParitySpinor ret, void *spinor, Precision cpu_prec, 
		 DiracFieldOrder dirac_order) 
{    
    if (ret.precision == QUDA_DOUBLE_PRECISION && cpu_prec != QUDA_DOUBLE_PRECISION) {
	printf("Error, cannot have CUDA double precision without double CPU precision\n");
	exit(-1);
    }
    
    if (ret.precision != QUDA_HALF_PRECISION) {
	
#ifndef __DEVICE_EMULATION__
    if (!packedSpinor1) cudaMallocHost(&packedSpinor1, ret.bytes);
#else
    if (!packedSpinor1) packedSpinor1 = malloc(ret.bytes);
#endif
    
    if (dirac_order == QUDA_DIRAC_ORDER || QUDA_CPS_WILSON_DIRAC_ORDER) {
	if (ret.precision == QUDA_DOUBLE_PRECISION) {
	    packParitySpinor((double2*)packedSpinor1, (double*)spinor, ret.volume);
	} else {
	    if (cpu_prec == QUDA_DOUBLE_PRECISION) {
		packParitySpinor((float2*)packedSpinor1, (double*)spinor, ret.volume);
	    }
	    else packParitySpinor((float2*)packedSpinor1, (float*)spinor, ret.volume);
	}
    } else if (dirac_order == QUDA_QDP_DIRAC_ORDER) {
	if (ret.precision == QUDA_DOUBLE_PRECISION) {
	packQDPParitySpinor((double2*)packedSpinor1, (double*)spinor, ret.volume);
	} else {
	    if (cpu_prec == QUDA_DOUBLE_PRECISION) packQDPParitySpinor((float4*)packedSpinor1, (double*)spinor, ret.volume);
	    else packQDPParitySpinor((float4*)packedSpinor1, (float*)spinor, ret.volume);
      }
    }
    cudaMemcpy(ret.spinor, packedSpinor1, ret.bytes, cudaMemcpyHostToDevice);
    } else {
	ParitySpinor tmp = allocateParitySpinor(ret.X, QUDA_SINGLE_PRECISION);
	loadParitySpinor(tmp, spinor, cpu_prec, dirac_order);
	copyCuda(ret, tmp);
	freeParitySpinor(tmp);
    }
    
}

void loadFullSpinor(FullSpinor ret, void *spinor, Precision cpu_prec) {

  if (ret.even.precision != QUDA_HALF_PRECISION) {
    
#ifndef __DEVICE_EMULATION__
    if (!packedSpinor1) cudaMallocHost(&packedSpinor1, ret.even.bytes);
    if (!packedSpinor2) cudaMallocHost(&packedSpinor2, ret.even.bytes);
#else
    if (!packedSpinor1) packedSpinor1 = malloc(ret.even.bytes);
    if (!packedSpinor2) packedSpinor2 = malloc(ret.even.bytes);
#endif
    
    if (ret.even.precision == QUDA_DOUBLE_PRECISION) {
      packFullSpinor((double2*)packedSpinor1, (double2*)packedSpinor2, (double*)spinor, ret.even.volume);
    } else {
      if (cpu_prec == QUDA_DOUBLE_PRECISION) 
	packFullSpinor((float4*)packedSpinor1, (float4*)packedSpinor2, (double*)spinor, ret.even.volume);
      else 
	packFullSpinor((float4*)packedSpinor1, (float4*)packedSpinor2, (float*)spinor, ret.even.volume);
    }
    
    cudaMemcpy(ret.even.spinor, packedSpinor1, ret.even.bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(ret.odd.spinor, packedSpinor2, ret.even.bytes, cudaMemcpyHostToDevice);

#ifndef __DEVICE_EMULATION__
    cudaFreeHost(packedSpinor2);
#else
    free(packedSpinor2);
#endif
    packedSpinor2 = 0;
  } else {
    FullSpinor tmp = allocateSpinorField(ret.even.X, QUDA_SINGLE_PRECISION);
    loadFullSpinor(tmp, spinor, cpu_prec);
    copyCuda(ret.even, tmp.even);
    copyCuda(ret.odd, tmp.odd);
    freeSpinorField(tmp);
  }

}

void loadSpinorField(FullSpinor ret, void *spinor, Precision cpu_prec, DiracFieldOrder dirac_order) 
{
    void *spinor_odd;
    if (cpu_prec == QUDA_SINGLE_PRECISION) spinor_odd = (float*)spinor + ret.even.length;
    else spinor_odd = (double*)spinor + ret.even.length;
    
    if (dirac_order == QUDA_LEX_DIRAC_ORDER) {
	loadFullSpinor(ret, spinor, cpu_prec);
  } else if (dirac_order == QUDA_DIRAC_ORDER || dirac_order == QUDA_QDP_DIRAC_ORDER) {
	loadParitySpinor(ret.even, spinor, cpu_prec, dirac_order);
	loadParitySpinor(ret.odd, spinor_odd, cpu_prec, dirac_order);
    } else if (dirac_order == QUDA_CPS_WILSON_DIRAC_ORDER) {
	// odd-even so reverse order
	loadParitySpinor(ret.even, spinor_odd, cpu_prec, dirac_order);
	loadParitySpinor(ret.odd, spinor, cpu_prec, dirac_order);
    } else {
	printf("DiracFieldOrder %d not supported\n", dirac_order);
	exit(-1);
  }
}

void retrieveParitySpinor(void *res, ParitySpinor spinor, Precision cpu_prec, DiracFieldOrder dirac_order) {

  if (spinor.precision != QUDA_HALF_PRECISION) {
    if (!packedSpinor1) cudaMallocHost((void**)&packedSpinor1, spinor.bytes);
    cudaMemcpy(packedSpinor1, spinor.spinor, spinor.bytes, cudaMemcpyDeviceToHost);

    if (dirac_order == QUDA_DIRAC_ORDER || QUDA_CPS_WILSON_DIRAC_ORDER) {
      if (spinor.precision == QUDA_DOUBLE_PRECISION) {
	unpackParitySpinor((double*)res, (double2*)packedSpinor1, spinor.volume);
      } else {
	if (cpu_prec == QUDA_DOUBLE_PRECISION) unpackParitySpinor((double*)res, (float2*)packedSpinor1, spinor.volume);
	else unpackParitySpinor((float*)res, (float2*)packedSpinor1, spinor.volume);
      }
    } else if (dirac_order == QUDA_QDP_DIRAC_ORDER) {
      if (spinor.precision == QUDA_DOUBLE_PRECISION) {
	unpackQDPParitySpinor((double*)res, (double2*)packedSpinor1, spinor.volume);
      } else {
	if (cpu_prec == QUDA_DOUBLE_PRECISION) unpackQDPParitySpinor((double*)res, (float4*)packedSpinor1, spinor.volume);
	else unpackQDPParitySpinor((float*)res, (float4*)packedSpinor1, spinor.volume);
      }
    }

  } else {
      ParitySpinor tmp = allocateParitySpinor(spinor.X, QUDA_SINGLE_PRECISION);
      copyCuda(tmp, spinor);
      retrieveParitySpinor(res, tmp, cpu_prec, dirac_order);
      freeParitySpinor(tmp);
  }
}

void retrieveFullSpinor(void *res, FullSpinor spinor, Precision cpu_prec) {

  if (spinor.even.precision != QUDA_HALF_PRECISION) {
    if (!packedSpinor1) cudaMallocHost((void**)&packedSpinor1, spinor.even.bytes);
    if (!packedSpinor2) cudaMallocHost((void**)&packedSpinor2, spinor.even.bytes);
    
    cudaMemcpy(packedSpinor1, spinor.even.spinor, spinor.even.bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(packedSpinor2, spinor.odd.spinor, spinor.odd.bytes, cudaMemcpyDeviceToHost);

    if (spinor.even.precision == QUDA_DOUBLE_PRECISION) {
      unpackFullSpinor((double*)res, (double2*)packedSpinor1, (double2*)packedSpinor2, spinor.even.volume);
    } else {
      if (cpu_prec == QUDA_DOUBLE_PRECISION) 
	unpackFullSpinor((double*)res, (float4*)packedSpinor1, (float4*)packedSpinor2, spinor.even.volume);
      else unpackFullSpinor((float*)res, (float4*)packedSpinor1, (float4*)packedSpinor2, spinor.even.volume);
    }
    
#ifndef __DEVICE_EMULATION__
    cudaFreeHost(packedSpinor2);
#else
    free(packedSpinor2);
#endif
    packedSpinor2 = 0;
  } else {
    FullSpinor tmp = allocateSpinorField(spinor.even.X, QUDA_SINGLE_PRECISION);
    copyCuda(tmp.even, spinor.even);
    copyCuda(tmp.odd, spinor.odd);
    retrieveFullSpinor(res, tmp, cpu_prec);
    freeSpinorField(tmp);
  }
}

void retrieveSpinorField(void *res, FullSpinor spinor, Precision cpu_prec, DiracFieldOrder dirac_order) 
{
    void *res_odd;
    if (cpu_prec == QUDA_SINGLE_PRECISION) res_odd = (float*)res + spinor.even.length;
    else res_odd = (double*)res + spinor.even.length;

    if (dirac_order == QUDA_LEX_DIRAC_ORDER) {
	retrieveFullSpinor(res, spinor, cpu_prec);
    } else if (dirac_order == QUDA_DIRAC_ORDER || dirac_order == QUDA_QDP_DIRAC_ORDER) {
	retrieveParitySpinor(res, spinor.even, cpu_prec, dirac_order);
	retrieveParitySpinor(res_odd, spinor.odd, cpu_prec, dirac_order);
    } else if (dirac_order == QUDA_CPS_WILSON_DIRAC_ORDER) {
	retrieveParitySpinor(res, spinor.odd, cpu_prec, dirac_order);
	retrieveParitySpinor(res_odd, spinor.even, cpu_prec, dirac_order);
    } else {
	printf("DiracFieldOrder %d not supported\n", dirac_order);
	exit(-1);
    }
  
}

/*
void spinorHalfPack(float *c, short *s0, float *f0, int V) {

  float *f = f0;
  short *s = s0;
  for (int i=0; i<24*V; i+=24) {
    c[i] = sqrt(f[0]*f[0] + f[1]*f[1]);
    for (int j=0; j<24; j+=2) {
      float k = sqrt(f[j]*f[j] + f[j+1]*f[j+1]);
      if (k > c[i]) c[i] = k;
    }

    for (int j=0; j<24; j++) s[j] = (short)(MAX_SHORT*f[j]/c[i]);
    f+=24;
    s+=24;
  }

}

void spinorHalfUnpack(float *f0, float *c, short *s0, int V) {
  float *f = f0;
  short *s = s0;
  for (int i=0; i<24*V; i+=24) {
    for (int j=0; j<24; j++) f[j] = s[j] * (c[i] / MAX_SHORT);
    f+=24;
    s+=24;
  }

}
*/
