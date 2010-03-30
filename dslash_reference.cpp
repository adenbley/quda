#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <quda.h>
#include <util_quda.h>
#include <dslash_reference.h>
#include "misc.h"
void *memset(void *s, int c, size_t n);

int Z[4];
int V;
int Vh;

void setDims(int *X) {
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    Z[d] = X[d];
  }
  Vh = V/2;
}

template <typename Float>
void sum(Float *dst, Float *a, Float *b, int cnt) {
  for (int i = 0; i < cnt; i++)
    dst[i] = a[i] + b[i];
}
template <typename Float>
void sub(Float *dst, Float *a, Float *b, int cnt) {
  for (int i = 0; i < cnt; i++)
    dst[i] = a[i] - b[i];
}
// performs the operation y[i] = x[i] + a*y[i]
template <typename Float>
void xpay(Float *x, Float a, Float *y, int len) {
    for (int i=0; i<len; i++) y[i] = x[i] + a*y[i];
}
// performs the operation y[i] = a*x[i] - y[i]
template <typename Float>
void axmy(Float *x, Float a, Float *y, int len) {
    for (int i=0; i<len; i++) y[i] = a*x[i] - y[i];
}

template <typename Float>
void negx(Float *x, int len) {
    for (int i=0; i<len; i++) x[i] = -x[i];
}

// i represents a "half index" into an even or odd "half lattice".
// when oddBit={0,1} the half lattice is {even,odd}.
// 
// the displacements, such as dx, refer to the full lattice coordinates. 
//
// neighborIndex() takes a "half index", displaces it, and returns the
// new "half index", which can be an index into either the even or odd lattices.
// displacements of magnitude one always interchange odd and even lattices.
//

int neighborIndex(int i, int oddBit, int dx4, int dx3, int dx2, int dx1) {
  int Y = fullLatticeIndex(i, oddBit);
  int x4 = Y/(Z[2]*Z[1]*Z[0]);
  int x3 = (Y/(Z[1]*Z[0])) % Z[2];
  int x2 = (Y/Z[0]) % Z[1];
  int x1 = Y % Z[0];
  
  // assert (oddBit == (x+y+z+t)%2);
  
  x4 = (x4+dx4+Z[3]) % Z[3];
  x3 = (x3+dx3+Z[2]) % Z[2];
  x2 = (x2+dx2+Z[1]) % Z[1];
  x1 = (x1+dx1+Z[0]) % Z[0];
  
  return (x4*(Z[2]*Z[1]*Z[0]) + x3*(Z[1]*Z[0]) + x2*(Z[0]) + x1) / 2;
}

template <typename Float>
Float *gaugeLink(int i, int dir, int oddBit, Float **gaugeEven, Float **gaugeOdd, int nbr_distance) {
  Float **gaugeField;
  int j;
  int d = nbr_distance;
  if (dir % 2 == 0) {
    j = i;
    gaugeField = (oddBit ? gaugeOdd : gaugeEven);
  }
  else {
    switch (dir) {
    case 1: j = neighborIndex(i, oddBit, 0, 0, 0, -d); break;
    case 3: j = neighborIndex(i, oddBit, 0, 0, -d, 0); break;
    case 5: j = neighborIndex(i, oddBit, 0, -d, 0, 0); break;
    case 7: j = neighborIndex(i, oddBit, -d, 0, 0, 0); break;
    default: j = -1; break;
    }
    gaugeField = (oddBit ? gaugeEven : gaugeOdd);
  }
  
  return &gaugeField[dir/2][j*(3*3*2)];
}



template <typename Float>
Float *spinorNeighbor(int i, int dir, int oddBit, Float *spinorField, int neighbor_distance) 
{
    int j;
    int nb = neighbor_distance;
    switch (dir) {
    case 0: j = neighborIndex(i, oddBit, 0, 0, 0, +nb); break;
    case 1: j = neighborIndex(i, oddBit, 0, 0, 0, -nb); break;
    case 2: j = neighborIndex(i, oddBit, 0, 0, +nb, 0); break;
    case 3: j = neighborIndex(i, oddBit, 0, 0, -nb, 0); break;
    case 4: j = neighborIndex(i, oddBit, 0, +nb, 0, 0); break;
    case 5: j = neighborIndex(i, oddBit, 0, -nb, 0, 0); break;
    case 6: j = neighborIndex(i, oddBit, +nb, 0, 0, 0); break;
    case 7: j = neighborIndex(i, oddBit, -nb, 0, 0, 0); break;
    default: j = -1; break;
    }
    
    return &spinorField[j*(spinorSiteSize)];
}

template <typename sFloat, typename gFloat>
void dot(sFloat* res, gFloat* a, sFloat* b) {
  res[0] = res[1] = 0;
  for (int m = 0; m < 3; m++) {
    sFloat a_re = a[2*m+0];
    sFloat a_im = a[2*m+1];
    sFloat b_re = b[2*m+0];
    sFloat b_im = b[2*m+1];
    res[0] += a_re * b_re - a_im * b_im;
    res[1] += a_re * b_im + a_im * b_re;
  }
}

template <typename Float>
void su3Transpose(Float *res, Float *mat) {
  for (int m = 0; m < 3; m++) {
    for (int n = 0; n < 3; n++) {
      res[m*(3*2) + n*(2) + 0] = + mat[n*(3*2) + m*(2) + 0];
      res[m*(3*2) + n*(2) + 1] = - mat[n*(3*2) + m*(2) + 1];
    }
  }
}


template <typename sFloat, typename gFloat>
void su3Mul(sFloat *res, gFloat *mat, sFloat *vec) {
  for (int n = 0; n < 3; n++) dot(&res[n*(2)], &mat[n*(3*2)], vec);
}

template <typename sFloat, typename gFloat>
void su3Tmul(sFloat *res, gFloat *mat, sFloat *vec) {
  gFloat matT[3*3*2];
  su3Transpose(matT, mat);
  su3Mul(res, matT, vec);
}


//
// dslashReference()
//
// if oddBit is zero: calculate even parity spinor elements (using odd parity spinor) 
// if oddBit is one:  calculate odd parity spinor elements 
//
// if daggerBit is zero: perform ordinary dslash operator
// if daggerBit is one:  perform hermitian conjugate of dslash
//
template<typename Float>
void display_link_internal(Float* link)
{
    int i, j;
    
    for (i = 0;i < 3; i++){
	for(j=0;j < 3; j++){
	    printf("(%10f,%10f) \t", link[i*3*2 + j*2], link[i*3*2 + j*2 + 1]);
	}
	printf("\n");
    }
    printf("\n");
    return;
}


template <typename sFloat, typename gFloat>
void dslashReference(sFloat *res, gFloat **fatlink, gFloat** longlink, sFloat *spinorField, int oddBit, int daggerBit) 
{
    for (int i=0; i<Vh*1*3*2; i++) res[i] = 0.0;
    
    gFloat *fatlinkEven[4], *fatlinkOdd[4];
    gFloat *longlinkEven[4], *longlinkOdd[4];
    
    for (int dir = 0; dir < 4; dir++) {  
	fatlinkEven[dir] = fatlink[dir];
	fatlinkOdd[dir] = fatlink[dir] + Vh*gaugeSiteSize;
	longlinkEven[dir] =longlink[dir];
	longlinkOdd[dir] = longlink[dir] + Vh*gaugeSiteSize;    
    }
    for (int i = 0; i < Vh; i++) {
	memset(res + i*spinorSiteSize, 0, spinorSiteSize*sizeof(sFloat));
	for (int dir = 0; dir < 8; dir++) {
	    gFloat* fatlnk = gaugeLink(i, dir, oddBit, fatlinkEven, fatlinkOdd, 1);
	    gFloat* longlnk = gaugeLink(i, dir, oddBit, longlinkEven, longlinkOdd, 3);

	    sFloat *first_neighbor_spinor = spinorNeighbor(i, dir, oddBit, spinorField, 1);
	    sFloat *third_neighbor_spinor = spinorNeighbor(i, dir, oddBit, spinorField, 3);
	    
	    sFloat gaugedSpinor[spinorSiteSize];

	    if (dir % 2 == 0){
		su3Mul(gaugedSpinor, fatlnk, first_neighbor_spinor);
		sum(&res[i*spinorSiteSize], &res[i*spinorSiteSize], gaugedSpinor, spinorSiteSize);	    
		su3Mul(gaugedSpinor, longlnk, third_neighbor_spinor);
		sum(&res[i*spinorSiteSize], &res[i*spinorSiteSize], gaugedSpinor, spinorSiteSize);		
	    }
	    else{
		su3Tmul(gaugedSpinor, fatlnk, first_neighbor_spinor);
		sub(&res[i*spinorSiteSize], &res[i*spinorSiteSize], gaugedSpinor, spinorSiteSize);       	
		
		su3Tmul(gaugedSpinor, longlnk, third_neighbor_spinor);
		sub(&res[i*spinorSiteSize], &res[i*spinorSiteSize], gaugedSpinor, spinorSiteSize);       
		
	    }	    	    
	}
	if (daggerBit){
	    negx(&res[i*spinorSiteSize], spinorSiteSize);
	}
    }
}


void dslash(void *res, void **fatlink, void** longlink, void *spinorField, int oddBit, int daggerBit,
	       Precision sPrecision, Precision gPrecision) {
    
    if (sPrecision == QUDA_DOUBLE_PRECISION) {
	if (gPrecision == QUDA_DOUBLE_PRECISION){
	    dslashReference((double*)res, (double**)fatlink, (double**)longlink, (double*)spinorField, oddBit, daggerBit);
	}else{
	    dslashReference((double*)res, (float**)fatlink, (float**)longlink, (double*)spinorField, oddBit, daggerBit);
	}
    }
    else{
	if (gPrecision == QUDA_DOUBLE_PRECISION){
	    dslashReference((float*)res, (double**)fatlink, (double**)longlink, (float*)spinorField, oddBit, daggerBit);
	}else{
	    dslashReference((float*)res, (float**)fatlink, (float**)longlink, (float*)spinorField, oddBit, daggerBit);
	}
    }
}



template <typename sFloat, typename gFloat>
void Mat(sFloat *out, gFloat **fatlink, gFloat** longlink, sFloat *in, sFloat kappa, int daggerBit) 
{
    sFloat *inEven = in;
    sFloat *inOdd  = in + Vh*spinorSiteSize;
    sFloat *outEven = out;
    sFloat *outOdd = out + Vh*spinorSiteSize;
    
    // full dslash operator
    dslashReference(outOdd, fatlink, longlink, inEven, 1, daggerBit);
    dslashReference(outEven, fatlink, longlink, inOdd, 0, daggerBit);
    
    // lastly apply the kappa term
    xpay(in, -kappa, out, V*spinorSiteSize);
}


void 
mat(void *out, void **fatlink, void** longlink, void *in, double kappa, int dagger_bit,
       Precision sPrecision, Precision gPrecision) 
{
    
    if (sPrecision == QUDA_DOUBLE_PRECISION){
	if (gPrecision == QUDA_DOUBLE_PRECISION) {
	    Mat((double*)out, (double**)fatlink, (double**)longlink, (double*)in, (double)kappa, dagger_bit);
	}else {
	    Mat((double*)out, (float**)fatlink, (float**)longlink, (double*)in, (double)kappa, dagger_bit);
	}
    }else{
	if (gPrecision == QUDA_DOUBLE_PRECISION){ 
	    Mat((float*)out, (double**)fatlink, (double**)longlink, (float*)in, (float)kappa, dagger_bit);
	}else {
	    Mat((float*)out, (float**)fatlink, (float**)longlink, (float*)in, (float)kappa, dagger_bit);
	}
    }
}



template <typename sFloat, typename gFloat>
void
Matdagmat_milc(sFloat *out, gFloat **fatlink, gFloat** longlink, sFloat *in, sFloat mass, int daggerBit, sFloat* tmp, QudaParity parity) 
{
 
    sFloat msq_x4 = mass*mass*4;

    switch(parity){
    case QUDA_EVEN:
	{
	    sFloat *inEven = in;
	    sFloat *outEven = out;
	    dslashReference(tmp, fatlink, longlink, inEven, 1, daggerBit);
	    dslashReference(outEven, fatlink, longlink, tmp, 0, daggerBit);
	    
	    // lastly apply the mass term
	    axmy(inEven, msq_x4, outEven, Vh*spinorSiteSize);
	    break;
	}
    case QUDA_ODD:
	{
	    sFloat *inOdd = in;
	    sFloat *outOdd = out;
	    dslashReference(tmp, fatlink, longlink, inOdd, 0, daggerBit);
	    dslashReference(outOdd, fatlink, longlink, tmp, 1, daggerBit);
	    
	    // lastly apply the mass term
	    axmy(inOdd, msq_x4, outOdd, Vh*spinorSiteSize);
	    break;	
	}
	
    case QUDA_EVENODD:
	{
	    sFloat *inEven = in;
	    sFloat *inOdd = in + Vh*spinorSiteSize;
	    sFloat *outEven = out;
	    sFloat *outOdd = out + Vh*spinorSiteSize;
	    sFloat *tmpEven = tmp;
	    sFloat *tmpOdd = tmp + Vh*spinorSiteSize;
	    
	    dslashReference(tmpOdd, fatlink, longlink, inEven, 1, daggerBit);
	    dslashReference(tmpEven, fatlink, longlink, inOdd, 0, daggerBit);
	    
	    dslashReference(outOdd, fatlink, longlink, tmpEven, 1, daggerBit);
	    dslashReference(outEven, fatlink, longlink, tmpOdd, 0, daggerBit);
	    
	    // lastly apply the mass term
	    axmy(in, msq_x4, out, V*spinorSiteSize);	    	    
	    break;
	}
    default:
	fprintf(stderr, "ERROR: invalid parity in %s,line %d\n", __FUNCTION__, __LINE__);
	break;
    }
    
}


void 
matdagmat_milc(void *out, void **fatlink, void** longlink, void *in, double mass, int dagger_bit,
	       Precision sPrecision, Precision gPrecision, void* tmp, QudaParity parity) 
{
    
    if (sPrecision == QUDA_DOUBLE_PRECISION){
	if (gPrecision == QUDA_DOUBLE_PRECISION) {
	    Matdagmat_milc((double*)out, (double**)fatlink, (double**)longlink, (double*)in, (double)mass, dagger_bit, (double*)tmp, parity);
	}else {
	    Matdagmat_milc((double*)out, (float**)fatlink, (float**)longlink, (double*)in, (double)mass, dagger_bit, (double*) tmp, parity);
	}
    }else{
	if (gPrecision == QUDA_DOUBLE_PRECISION){ 
	    Matdagmat_milc((float*)out, (double**)fatlink, (double**)longlink, (float*)in, (float)mass, dagger_bit, (float*)tmp, parity);
	}else {
	    Matdagmat_milc((float*)out, (float**)fatlink, (float**)longlink, (float*)in, (float)mass, dagger_bit, (float*)tmp, parity);
	}
    }
}



// Apply the even-odd preconditioned Dirac operator
template <typename sFloat, typename gFloat>
void MatPC(sFloat *outEven, gFloat **fatlink, gFloat** longlink, sFloat *inEven, sFloat kappa, 
	      int daggerBit, MatPCType matpc_type) {
    
    sFloat *tmp = (sFloat*)malloc(Vh*spinorSiteSize*sizeof(sFloat));
    
    // full dslash operator
    if (matpc_type == QUDA_MATPC_EVEN_EVEN) {
	dslashReference(tmp, fatlink, longlink, inEven, 1, daggerBit);
	dslashReference(outEven, fatlink, longlink, tmp, 0, daggerBit);
    } else {
	dslashReference(tmp, fatlink, longlink, inEven, 0, daggerBit);
	dslashReference(outEven, fatlink, longlink, tmp, 1, daggerBit);
    }    
  
    // lastly apply the kappa term
    sFloat kappa2 = -kappa*kappa;
    xpay(inEven, kappa2, outEven, Vh*spinorSiteSize);
    free(tmp);
}


void
matpc(void *outEven, void **fatlink, void**longlink, void *inEven, double kappa, 
	 MatPCType matpc_type, int dagger_bit, Precision sPrecision, Precision gPrecision) 
{
    
    if (sPrecision == QUDA_DOUBLE_PRECISION)
	if (gPrecision == QUDA_DOUBLE_PRECISION) {
	    MatPC((double*)outEven, (double**)fatlink, (double**)longlink, (double*)inEven, (double)kappa, dagger_bit, matpc_type);
	}
	else{
	    MatPC((double*)outEven, (double**)fatlink, (double**)longlink, (double*)inEven, (double)kappa, dagger_bit, matpc_type);
	}
    else {
	if (gPrecision == QUDA_DOUBLE_PRECISION){ 
	    MatPC((float*)outEven, (double**)fatlink, (double**)longlink, (float*)inEven, (float)kappa, dagger_bit, matpc_type);
	}else{
	    MatPC((float*)outEven, (float**)fatlink, (float**)longlink, (float*)inEven, (float)kappa, dagger_bit, matpc_type);
	}
    }
}
