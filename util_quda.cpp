// util_quda.cpp
// Ver. 09.10.c

// 10/19/09:  Most recent is now on memstick.
// 10/22/09:  Changed getOddBit back to 4d parity.

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include <quda.h>
#include <util_quda.h>
#include <dslash_reference.h>
#include <myerror.h>

#include <complex>

using namespace std;

//#define DBUG

struct timeval startTime;

//ok
void stopwatchStart() {
  gettimeofday(&startTime, NULL);
}

//ok
double stopwatchReadSeconds() {
  struct timeval endTime;
  gettimeofday( &endTime, 0);
    
  long ds = endTime.tv_sec - startTime.tv_sec;
  //J  Pretty clear that usec is microseconds.
  long dus = endTime.tv_usec - startTime.tv_usec;
  return ds + 0.000001*dus;
}

#define SHORT_LENGTH 65536
#define SCALE_FLOAT ((SHORT_LENGTH-1) / 2.0)
#define SHIFT_FLOAT (-1.f / (SHORT_LENGTH-1))

template <typename Float>
inline short FloatToShort(Float a) {
  //return (short)(a*MAX_SHORT);
  short rtn = (short)((a+SHIFT_FLOAT)*SCALE_FLOAT);
  return rtn;
}

template <typename Float>
inline Float shortToFloat(short a) {
  Float rtn = (float)a/SCALE_FLOAT - SHIFT_FLOAT;
  return rtn;
}

template <typename Float>
void printVector(Float *v) {
  printf("{(%f %f) (%f %f) (%f %f)}\n", v[0], v[1], v[2], v[3], v[4], v[5]);
}

// X indexes the full lattice
void printSpinorElement(void *spinor, int X, Precision precision) {
  if (getOddBit_5d(X) == 0) {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int s=0; s<4; s++) printVector((double*)spinor+(X/2)*4*3*2+s*(3*2));
    else
      for (int s=0; s<4; s++) printVector((float*)spinor+(X/2)*4*3*2+s*(3*2));
  } else {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int s=0; s<4; s++) printVector((double*)spinor+(X/2)*(4*3*2)+Nh_5d*spinorSiteSize+s*(3*2));
    else
      for (int s=0; s<4; s++) printVector((float*)spinor+(X/2)*(4*3*2)+Nh_5d*spinorSiteSize+s*(3*2));
  }
}

// X indexes the full lattice
void printGaugeElement(void *gauge, int X, Precision precision) {
  if (getOddBit_4d(X) == 0) {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int m=0; m<3; m++) printVector((double*)gauge +(X/2)*gaugeSiteSize + m*3*2);
    else
      for (int m=0; m<3; m++) printVector((float*)gauge +(X/2)*gaugeSiteSize + m*3*2);
      
  } else {
    if (precision == QUDA_DOUBLE_PRECISION)
      for (int m = 0; m < 3; m++) printVector((double*)gauge + (X/2+Nh_4d)*gaugeSiteSize + m*3*2);
    else
      for (int m = 0; m < 3; m++) printVector((float*)gauge + (X/2+Nh_4d)*gaugeSiteSize + m*3*2);
  }
}

// returns 0 or 1 if the full lattice index X is even or odd
// 4d parity is determined here.
int getOddBit_4d(int X) {

  int x4 = (X/(L3*L2*L1)) % L4;
  int x3 = (X/(L2*L1)) % L3;
  int x2 = (X/L1) % L2;
  int x1 = X % L1;

  return (x4+x3+x2+x1) % 2;
}

// returns 0 or 1 if the full lattice index X is even or odd
// 5d parity is determined here.
int getOddBit_5d(int X) {

  int xs = X/(L4*L3*L2*L1);
  int x4 = (X/(L3*L2*L1)) % L4;
  int x3 = (X/(L2*L1)) % L3;
  int x2 = (X/L1) % L2;
  int x1 = X % L1;

  return (xs+x4+x3+x2+x1) % 2;
}

// a+=b
template <typename Float>
void complexAddTo(Float *a, Float *b) {
  a[0] += b[0];
  a[1] += b[1];
}

// a = b*c
template <typename Float>
void complexProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = b[0]*c[1] + b[1]*c[0];
}

// a = conj(b)*conj(c)
template <typename Float>
void complexConjugateProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] - b[1]*c[1];
    a[1] = -b[0]*c[1] - b[1]*c[0];
}

// a = conj(b)*c
template <typename Float>
void complexDotProduct(Float *a, Float *b, Float *c) {
    a[0] = b[0]*c[0] + b[1]*c[1];
    a[1] = b[0]*c[1] - b[1]*c[0];
}

// a += b*c
template <typename Float>
void accumulateComplexProduct(Float *a, Float *b, Float *c, Float sign) {
  a[0] += sign*(b[0]*c[0] - b[1]*c[1]);
  a[1] += sign*(b[0]*c[1] + b[1]*c[0]);
}

// a += conj(b)*c)
template <typename Float>
void accumulateComplexDotProduct(Float *a, Float *b, Float *c) {
    a[0] += b[0]*c[0] + b[1]*c[1];
    a[1] += b[0]*c[1] - b[1]*c[0];
}

template <typename Float>
void accumulateConjugateProduct(Float *a, Float *b, Float *c, int sign) {
  a[0] += sign * (b[0]*c[0] - b[1]*c[1]);
  a[1] -= sign * (b[0]*c[1] + b[1]*c[0]);
}

template <typename Float>
void su3Construct12(Float *mat) {
  Float *w = mat+12;
  w[0] = 0.0;
  w[1] = 0.0;
  w[2] = 0.0;
  w[3] = 0.0;
  w[4] = 0.0;
  w[5] = 0.0;
}

// Stabilized Bunk and Sommer
template <typename Float>
void su3Construct8(Float *mat) {
  mat[0] = atan2(mat[1], mat[0]);
  mat[1] = atan2(mat[13], mat[12]);
  for (int i=8; i<18; i++) mat[i] = 0.0;
}

void su3_construct(void *mat, ReconstructType reconstruct, Precision precision) {
  if (reconstruct == QUDA_RECONSTRUCT_12) {
    if (precision == QUDA_DOUBLE_PRECISION) su3Construct12((double*)mat);
    else su3Construct12((float*)mat);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) su3Construct8((double*)mat);
    else su3Construct8((float*)mat);
  }
}

// given first two rows (u,v) of SU(3) matrix mat, reconstruct the third row
// as the cross product of the conjugate vectors: w = u* x v*
// 
// 48 flops
template <typename Float>
void su3Reconstruct12(Float *mat, int dir, int ga_idx) {
  Float *u = &mat[0*(3*2)];
  Float *v = &mat[1*(3*2)];
  Float *w = &mat[2*(3*2)];
  w[0] = 0.0; w[1] = 0.0; w[2] = 0.0; w[3] = 0.0; w[4] = 0.0; w[5] = 0.0;
  accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
  accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
  accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
  accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
  accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
  accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
  Float u0 = (dir < 3 ? gauge_param->anisotropy :
	      (ga_idx >= (L4-1)*L1h*L2*L3 ? gauge_param->t_boundary : 1));
  w[0]*=u0; w[1]*=u0; w[2]*=u0; w[3]*=u0; w[4]*=u0; w[5]*=u0;
}

template <typename Float>
void su3Reconstruct8(Float *mat, int dir, int ga_idx) {
  // First reconstruct first row
  Float row_sum = 0.0;
  row_sum += mat[2]*mat[2];
  row_sum += mat[3]*mat[3];
  row_sum += mat[4]*mat[4];
  row_sum += mat[5]*mat[5];
  Float u0 = (dir < 3 ? gauge_param->anisotropy :
	      (ga_idx >= (L4-1)*L1h*L2*L3 ? gauge_param->t_boundary : 1));
  Float U00_mag = sqrt(1.f/(u0*u0) - row_sum);

  mat[14] = mat[0];
  mat[15] = mat[1];

  mat[0] = U00_mag * cos(mat[14]);
  mat[1] = U00_mag * sin(mat[14]);

  Float column_sum = 0.0;
  for (int i=0; i<2; i++) column_sum += mat[i]*mat[i];
  for (int i=6; i<8; i++) column_sum += mat[i]*mat[i];
  Float U20_mag = sqrt(1.f/(u0*u0) - column_sum);

  mat[12] = U20_mag * cos(mat[15]);
  mat[13] = U20_mag * sin(mat[15]);

  // First column now restored

  // finally reconstruct last elements from SU(2) rotation
  Float r_inv2 = 1.0/(u0*row_sum);

  // U11
  Float A[2];
  complexDotProduct(A, mat+0, mat+6);
  complexConjugateProduct(mat+8, mat+12, mat+4);
  accumulateComplexProduct(mat+8, A, mat+2, u0);
  mat[8] *= -r_inv2;
  mat[9] *= -r_inv2;

  // U12
  complexConjugateProduct(mat+10, mat+12, mat+2);
  accumulateComplexProduct(mat+10, A, mat+4, -u0);
  mat[10] *= r_inv2;
  mat[11] *= r_inv2;

  // U21
  complexDotProduct(A, mat+0, mat+12);
  complexConjugateProduct(mat+14, mat+6, mat+4);
  accumulateComplexProduct(mat+14, A, mat+2, -u0);
  mat[14] *= r_inv2;
  mat[15] *= r_inv2;

  // U12
  complexConjugateProduct(mat+16, mat+6, mat+2);
  accumulateComplexProduct(mat+16, A, mat+4, u0);
  mat[16] *= -r_inv2;
  mat[17] *= -r_inv2;
}

void su3_reconstruct(void *mat, int dir, int ga_idx, ReconstructType reconstruct, Precision precision) {
  if (reconstruct == QUDA_RECONSTRUCT_12) {
    if (precision == QUDA_DOUBLE_PRECISION) su3Reconstruct12((double*)mat, dir, ga_idx);
    else su3Reconstruct12((float*)mat, dir, ga_idx);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) su3Reconstruct8((double*)mat, dir, ga_idx);
    else su3Reconstruct8((float*)mat, dir, ga_idx);
  }
}

/*
void su3_construct_8_half(float *mat, short *mat_half) {
  su3Construct8(mat);

  mat_half[0] = floatToShort(mat[0] / M_PI);
  mat_half[1] = floatToShort(mat[1] / M_PI);
  for (int i=2; i<18; i++) {
    mat_half[i] = floatToShort(mat[i]);
  }
}

void su3_reconstruct_8_half(float *mat, short *mat_half, int dir, int ga_idx) {

  for (int i=0; i<18; i++) {
    mat[i] = shortToFloat(mat_half[i]);
  }
  mat[0] *= M_PI;
  mat[1] *= M_PI;

  su3Reconstruct8(mat, dir, ga_idx);
  }*/

template <typename Float>
int compareFloats(Float *a, Float *b, int len, double epsilon) {
  for (int i = 0; i < len; i++) {
    double diff = fabs(a[i] - b[i]);
    if (diff > epsilon) return 0;
  }
  return 1;
}

int compare_floats(void *a, void *b, int len, double epsilon, Precision precision) {
  if  (precision == QUDA_DOUBLE_PRECISION) return compareFloats((double*)a, (double*)b, len, epsilon);
  else return compareFloats((float*)a, (float*)b, len, epsilon);
}

// 4d checkerboard.
// given a "half index" i into either an even or odd half lattice (corresponding
// to oddBit = {0, 1}), returns the corresponding full lattice index.
// Cf. GPGPU code in dslash_core_ante.h.
// There, i is the thread index.
int fullLatticeIndex_4d(int i, int oddBit) {
  if (i >= Nh_4d || i < 0) myerror("i out of range in fullLatticeIndex_4d");
  int boundaryCrossings = i/L1h + i/(L2*L1h) + i/(L3*L2*L1h);
  return 2*i + (boundaryCrossings + oddBit) % 2;
}

// 5d checkerboard.
// given a "half index" i into either an even or odd half lattice (corresponding
// to oddBit = {0, 1}), returns the corresponding full lattice index.
// Cf. GPGPU code in dslash_core_ante.h.
// There, i is the thread index sid.
// This function is used by neighborIndex_5d in dslash_reference.cpp.
//ok
int fullLatticeIndex_5d(int i, int oddBit) {
  int boundaryCrossings = i/L1h + i/(L2*L1h) + i/(L3*L2*L1h)+ i/(L4*L3*L2*L1h);
  return 2*i + (boundaryCrossings + oddBit) % 2;
}

template <typename Float>
void applyGaugeFieldScaling(Float **gauge) {
  // Apply spatial scaling factor (u0) to spatial links
  for (int d = 0; d < 3; d++) {
    for (int i = 0; i < gaugeSiteSize*N_4d; i++) {
      gauge[d][i] /= gauge_param->anisotropy;
    }
  }
    
  // Apply boundary conditions to temporal links
  if (gauge_param->t_boundary == QUDA_ANTI_PERIODIC_T) {
    for (int j = L1h*L2*L3*(L4-1); j < Nh_4d; j++) {
      for (int i = 0; i < gaugeSiteSize; i++) {
	gauge[3][j*gaugeSiteSize+i] *= -1.0;
	gauge[3][(Nh_4d+j)*gaugeSiteSize+i] *= -1.0;
      }
    }
  }
    
  if (gauge_param->gauge_fix) {
    // set all gauge links (except for the first L1h*L2*L3) to the identity,
    // to simulate fixing to the temporal gauge.
    int dir = 3; // time direction only
    Float *even = gauge[dir];
    Float *odd  = gauge[dir]+Nh_4d*gaugeSiteSize;
    for (int i = L1h*L2*L3; i < Nh_4d; i++) {
      for (int m = 0; m < 3; m++) {
	for (int n = 0; n < 3; n++) {
	  even[i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  even[i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	  odd [i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  odd [i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	}
      }
    }
  }
}

template <typename Float>
void constructUnitGaugeField(Float **res) {
  Float *resOdd[4], *resEven[4];
  for (int dir = 0; dir < 4; dir++) {  
    resEven[dir] = res[dir];
    resOdd[dir]  = res[dir]+Nh_4d*gaugeSiteSize;
  }
    
  for (int dir = 0; dir < 4; dir++) {
    for (int i = 0; i < Nh_4d; i++) {
      for (int m = 0; m < 3; m++) {
	for (int n = 0; n < 3; n++) {
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = (m==n) ? 1 : 0;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = 0.0;
	}
      }
    }
  }
    
  applyGaugeFieldScaling(res);
}

// normalize the vector a
template <typename Float>
void normalize(complex<Float> *a, int len) {
  double sum = 0.0;
  for (int i=0; i<len; i++) sum += norm(a[i]);
  for (int i=0; i<len; i++) a[i] /= sqrt(sum);
}

// orthogonalize vector b to vector a
template <typename Float>
void orthogonalize(complex<Float> *a, complex<Float> *b, int len) {
  complex<double> dot = 0.0;
  for (int i=0; i<len; i++) dot += conj(a[i])*b[i];
  for (int i=0; i<len; i++) b[i] -= (complex<Float>)dot*a[i];
}

template <typename Float> 
void constructGaugeField(Float **res) {
  Float *resOdd[4], *resEven[4];
  for (int dir = 0; dir < 4; dir++) {  
    resEven[dir] = res[dir];
    resOdd[dir]  = res[dir]+Nh_4d*gaugeSiteSize;
  }
    
  for (int dir = 0; dir < 4; dir++) {
    for (int i = 0; i < Nh_4d; i++) {
      for (int m = 1; m < 3; m++) { // last 2 rows
	for (int n = 0; n < 3; n++) { // 3 columns
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
	  resEven[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 0] = rand() / (Float)RAND_MAX;
	  resOdd[dir][i*(3*3*2) + m*(3*2) + n*(2) + 1] = rand() / (Float)RAND_MAX;                    
	}
      }
      normalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), 3);
      orthogonalize((complex<Float>*)(resEven[dir] + (i*3+1)*3*2), (complex<Float>*)(resEven[dir] + (i*3+2)*3*2), 3);
      normalize((complex<Float>*)(resEven[dir] + (i*3 + 2)*3*2), 3);
      
      normalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), 3);
      orthogonalize((complex<Float>*)(resOdd[dir] + (i*3+1)*3*2), (complex<Float>*)(resOdd[dir] + (i*3+2)*3*2), 3);
      normalize((complex<Float>*)(resOdd[dir] + (i*3 + 2)*3*2), 3);

      {
	Float *w = resEven[dir]+(i*3+0)*3*2;
	Float *u = resEven[dir]+(i*3+1)*3*2;
	Float *v = resEven[dir]+(i*3+2)*3*2;
	
	for (int n = 0; n < 6; n++) w[n] = 0.0;
	accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
	accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
	accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
	accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
	accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
	accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
      }

      {
	Float *w = resOdd[dir]+(i*3+0)*3*2;
	Float *u = resOdd[dir]+(i*3+1)*3*2;
	Float *v = resOdd[dir]+(i*3+2)*3*2;
	
	for (int n = 0; n < 6; n++) w[n] = 0.0;
	accumulateConjugateProduct(w+0*(2), u+1*(2), v+2*(2), +1);
	accumulateConjugateProduct(w+0*(2), u+2*(2), v+1*(2), -1);
	accumulateConjugateProduct(w+1*(2), u+2*(2), v+0*(2), +1);
	accumulateConjugateProduct(w+1*(2), u+0*(2), v+2*(2), -1);
	accumulateConjugateProduct(w+2*(2), u+0*(2), v+1*(2), +1);
	accumulateConjugateProduct(w+2*(2), u+1*(2), v+0*(2), -1);
      }

    }
  }
    
  applyGaugeFieldScaling(res);
}

void construct_gauge_field(void **gauge, int type, Precision precision) {
  if (type == 0) {
    if (precision == QUDA_DOUBLE_PRECISION) constructUnitGaugeField((double**)gauge);
    else constructUnitGaugeField((float**)gauge);
   } else {
    if (precision == QUDA_DOUBLE_PRECISION) constructGaugeField((double**)gauge);
    else constructGaugeField((float**)gauge);
  }
}

template <typename Float>
void constructPointSpinorField(Float *res, int i0, int s0, int c0) {
  Float *resEven = res;
  Float *resOdd = res + Nh_5d*spinorSiteSize;
    
  for(int i = 0; i < Nh_5d; i++) {
    for (int s = 0; s < 4; s++) {
      for (int m = 0; m < 3; m++) {
	resEven[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 0;
	resEven[i*(4*3*2) + s*(3*2) + m*(2) + 1] = 0;
	resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 0;
	resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 1] = 0;
	if (s == s0 && m == c0) {
	  if (fullLatticeIndex_5d(i, 0) == i0)
	    resEven[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 1;
	  if (fullLatticeIndex_5d(i, 1) == i0)
	    resOdd[i*(4*3*2) + s*(3*2) + m*(2) + 0] = 1;
	}
      }
    }
  }
}

template <typename Float>
void constructSpinorField(Float *res) {
  for(int i = 0; i < N_5d; i++) {
    for (int s = 0; s < 4; s++) {
      for (int m = 0; m < 3; m++) {
	res[i*(4*3*2) + s*(3*2) + m*(2) + 0] = rand() / (Float)RAND_MAX;
	res[i*(4*3*2) + s*(3*2) + m*(2) + 1] = rand() / (Float)RAND_MAX;
      }
    }
  }
}

// Presently, dslash_test_dwf.c calls this with type=1 for initialization of the input spinor.
void construct_spinor_field(void *spinor, int type, int i0, int s0, int c0, Precision precision) {
  if (type == 0) {
    if (precision == QUDA_DOUBLE_PRECISION) constructPointSpinorField((double*)spinor, i0, s0, c0);
    else constructPointSpinorField((float*)spinor, i0, s0, c0);
  } else {
    if (precision == QUDA_DOUBLE_PRECISION) constructSpinorField((double*)spinor);
    else constructSpinorField((float*)spinor);
    // debugging code...
#ifdef DBUG
    printf("Input spinor:\n");
    printSpinorElement(spinor, 0, precision); printf("\n");
    printSpinorElement(spinor, Nh_5d-1, precision); printf("\n");    
    printSpinorElement(spinor, Nh_5d, precision); printf("\n");    
    printSpinorElement(spinor, N_5d-1, precision); printf("\n");    
#endif    
  }
}

template <typename Float>
void applyGamma5(Float *out, Float *in, int sites) {
  for (int i=0; i<sites*spinorSiteSize; i+=spinorSiteSize) {
    for (int j=0; j<spinorSiteSize/2; j++) 
      out[i+j] = in[i+j];
    for (int j=0; j<spinorSiteSize/2; j++) 
      out[i+j+spinorSiteSize/2] = -in[i+j+spinorSiteSize/2];
  }
}

void apply_gamma5(void *out, void *in, int sites, Precision precision) {
  if (precision == QUDA_DOUBLE_PRECISION) applyGamma5((double*)out, (double*)in, sites);
  else applyGamma5((float*)out, (float*)in, sites);
}

template <typename Float>
void compareSpinor(Float *spinorRef, Float *spinorGPU, int len) {
  int fail_check = 16;
  int fail[fail_check];
  for (int f=0; f<fail_check; f++) fail[f] = 0;

  int iter[24];
  for (int i=0; i<24; i++) iter[i] = 0;

  for (int i=0; i<len; i++) {
    for (int j=0; j<24; j++) {
      int is = i*24+j;
      double diff = fabs(spinorRef[is]-spinorGPU[is]);
      for (int f=0; f<fail_check; f++)
	if (diff > pow(10.0,-(f+1))) fail[f]++;
      //if (diff > 1e-1) printf("%d %d %e\n", i, j, diff);
      if (diff > 1e-3) iter[j]++;
    }
  }
    
  for (int i=0; i<24; i++) printf("%d fails = %d\n", i, iter[i]);
    
  for (int f=0; f<fail_check; f++) {
    printf("%e Failures: %d / %d  = %e\n", pow(10.0,-(f+1)), fail[f], len*24, fail[f] / (double)(len*24));
  }

}

void compare_spinor(void *spinor_ref, void *spinor_gpu, int len, Precision precision) {
  if (precision == QUDA_DOUBLE_PRECISION) compareSpinor((double*)spinor_ref, (double*)spinor_gpu, len);
  else compareSpinor((float*)spinor_ref, (float*)spinor_gpu, len);
}

// Prints 1st and last.
void strong_check(void *spinorRef, void *spinorGPU, int len, Precision prec) {
  printf("Reference:\n");
  printSpinorElement(spinorRef, 0, prec); printf("...\n");
  printSpinorElement(spinorRef, len-1, prec); printf("\n");    
    
  printf("\nCUDA:\n");
  printSpinorElement(spinorGPU, 0, prec); printf("...\n");
  printSpinorElement(spinorGPU, len-1, prec); printf("\n");

  compare_spinor(spinorRef, spinorGPU, len, prec);
}


