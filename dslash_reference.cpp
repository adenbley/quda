// dslash_reference.cpp
// Ver. 09.10.f

// 10/19/09:  Most recent now on memstick.
// 10/22/09:  Added some error checking, myerror.h so I can call my error function.
// 10/28/09:  Began to enable single precision.
// 12/26/09:  Modifications needed for the 5dPC.  Began editing process.
//   Will also need all of the Mat and MatPC ref. code working for testing
//   the GPU code.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <myerror.h>
#include <quda.h>
#include <util_quda.h>
#include <dslash_reference.h>

template <typename Float>
void sum(Float *dst, Float *a, Float *b, int cnt) {
  for (int i = 0; i < cnt; i++)
    dst[i] = a[i] + b[i];
}

template <typename Float>
void product(Float *dst, Float a, Float *b, int cnt) {
  for (int i = 0; i < cnt; i++)
    dst[i] = a * b[i];
}

// performs the operation y[i] = x[i] + a*y[i]
template <typename Float>
void xpay(Float *x, Float a, Float *y, int len) {
    for (int i=0; i<len; i++) y[i] = x[i] + a*y[i];
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
//
int neighborIndex_5d(int i, int oddBit, int dxs, int dx4, int dx3, int dx2, int dx1) {
  // fullLatticeIndex was modified for fullLatticeIndex_4d.  It is in util_quda.cpp.
  // This code bit may not properly perform 5dPC.
  int X = fullLatticeIndex_5d(i, oddBit);
  // Checked that this matches code in dslash_core_ante.h.
  int xs = X/(L4*L3*L2*L1);
  int x4 = (X/(L3*L2*L1)) % L4;
  int x3 = (X/(L2*L1)) % L3;
  int x2 = (X/L1) % L2;
  int x1 = X % L1;
  // Displace and project back into domain 0,...,Ls-1.
  // Note that we add Ls to avoid the negative problem
  // of the C % operator.
  xs = (xs+dxs+Ls) % Ls;
  // Etc.
  x4 = (x4+dx4+L4) % L4;
  x3 = (x3+dx3+L3) % L3;
  x2 = (x2+dx2+L2) % L2;
  x1 = (x1+dx1+L1) % L1;
  // Return linear half index.  Remember that integer division
  // rounds down.
  return (xs*(L4*L3*L2*L1) + x4*(L3*L2*L1) + x3*(L2*L1) + x2*(L1) + x1) / 2;
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
//
int neighborIndex_4d(int i, int oddBit, int dx4, int dx3, int dx2, int dx1) {
  // On input i should be in the range [0 , ... , L1h*L2*L3*L4-1].
  if (i < 0 || i >= (L1h*L2*L3*L4)) myerror("i out of range in neighborIndex_4d");
  // Compute the linear index.  Then dissect.
  // fullLatticeIndex_4d is in util_quda.cpp.
  // The gauge fields live on a 4d sublattice.  
  int X = fullLatticeIndex_4d(i, oddBit);
  int x4 = X/(L3*L2*L1);
  int x3 = (X/(L2*L1)) % L3;
  int x2 = (X/L1) % L2;
  int x1 = X % L1;
  
  x4 = (x4+dx4+L4) % L4;
  x3 = (x3+dx3+L3) % L3;
  x2 = (x2+dx2+L2) % L2;
  x1 = (x1+dx1+L1) % L1;
  
  return (x4*(L3*L2*L1) + x3*(L2*L1) + x2*(L1) + x1) / 2;
}

// This is just a copy of gaugeLink() from the quda code, except
// that neighborIndex() is replaced by the renamed version
// neighborIndex_4d().
//ok
template <typename Float>
Float *gaugeLink(int i, int dir, int oddBit, Float **gaugeEven,
                Float **gaugeOdd) {
  Float **gaugeField;
  int j;
  
  // If going forward, just grab link at site, U_\mu(x).
  if (dir % 2 == 0) {
    j = i;
    // j will get used in the return statement below.
    gaugeField = (oddBit ? gaugeOdd : gaugeEven);
  } else {
    // If going backward, a shift must occur, U_\mu(x-\muhat)^\dagger;
    // dagger happens elsewhere, here we're just doing index gymnastics.
    switch (dir) {
    case 1: j = neighborIndex_4d(i, oddBit, 0, 0, 0, -1); break;
    case 3: j = neighborIndex_4d(i, oddBit, 0, 0, -1, 0); break;
    case 5: j = neighborIndex_4d(i, oddBit, 0, -1, 0, 0); break;
    case 7: j = neighborIndex_4d(i, oddBit, -1, 0, 0, 0); break;
    default: j = -1; break;
    }
    gaugeField = (oddBit ? gaugeEven : gaugeOdd);
  }
  
  return &gaugeField[dir/2][j*(3*3*2)];
}

template <typename Float>
Float *spinorNeighbor_5d(int i, int dir, int oddBit, Float *spinorField) {
  int j;
  switch (dir) {
  case 0: j = neighborIndex_5d(i, oddBit, 0, 0, 0, 0, +1); break;
  case 1: j = neighborIndex_5d(i, oddBit, 0, 0, 0, 0, -1); break;
  case 2: j = neighborIndex_5d(i, oddBit, 0, 0, 0, +1, 0); break;
  case 3: j = neighborIndex_5d(i, oddBit, 0, 0, 0, -1, 0); break;
  case 4: j = neighborIndex_5d(i, oddBit, 0, 0, +1, 0, 0); break;
  case 5: j = neighborIndex_5d(i, oddBit, 0, 0, -1, 0, 0); break;
  case 6: j = neighborIndex_5d(i, oddBit, 0, +1, 0, 0, 0); break;
  case 7: j = neighborIndex_5d(i, oddBit, 0, -1, 0, 0, 0); break;
  case 8: j = neighborIndex_5d(i, oddBit, +1, 0, 0, 0, 0); break;
  case 9: j = neighborIndex_5d(i, oddBit, -1, 0, 0, 0, 0); break;
  default: j = -1; break;
  }
  
  return &spinorField[j*(4*3*2)];
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

//J  Directions 0..7 were used in the 4d code.
//J  Directions 8,9 will be for P_- and P_+, chiral
//J  projectors.
const double projector[10][4][4][2] = {
  {
    {{1,0}, {0,0}, {0,0}, {0,-1}},
    {{0,0}, {1,0}, {0,-1}, {0,0}},
    {{0,0}, {0,1}, {1,0}, {0,0}},
    {{0,1}, {0,0}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {0,0}, {0,1}},
    {{0,0}, {1,0}, {0,1}, {0,0}},
    {{0,0}, {0,-1}, {1,0}, {0,0}},
    {{0,-1}, {0,0}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {0,0}, {1,0}},
    {{0,0}, {1,0}, {-1,0}, {0,0}},
    {{0,0}, {-1,0}, {1,0}, {0,0}},
    {{1,0}, {0,0}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {0,0}, {-1,0}},
    {{0,0}, {1,0}, {1,0}, {0,0}},
    {{0,0}, {1,0}, {1,0}, {0,0}},
    {{-1,0}, {0,0}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {0,-1}, {0,0}},
    {{0,0}, {1,0}, {0,0}, {0,1}},
    {{0,1}, {0,0}, {1,0}, {0,0}},
    {{0,0}, {0,-1}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {0,1}, {0,0}},
    {{0,0}, {1,0}, {0,0}, {0,-1}},
    {{0,-1}, {0,0}, {1,0}, {0,0}},
    {{0,0}, {0,1}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {-1,0}, {0,0}},
    {{0,0}, {1,0}, {0,0}, {-1,0}},
    {{-1,0}, {0,0}, {1,0}, {0,0}},
    {{0,0}, {-1,0}, {0,0}, {1,0}}
  },
  {
    {{1,0}, {0,0}, {1,0}, {0,0}},
    {{0,0}, {1,0}, {0,0}, {1,0}},
    {{1,0}, {0,0}, {1,0}, {0,0}},
    {{0,0}, {1,0}, {0,0}, {1,0}}
  },
  // P_+ = P_R
  {
    {{2,0}, {0,0}, {0,0}, {0,0}},
    {{0,0}, {2,0}, {0,0}, {0,0}},
    {{0,0}, {0,0}, {0,0}, {0,0}},
    {{0,0}, {0,0}, {0,0}, {0,0}}
  },
  // P_- = P_L
  {
    {{0,0}, {0,0}, {0,0}, {0,0}},
    {{0,0}, {0,0}, {0,0}, {0,0}},
    {{0,0}, {0,0}, {2,0}, {0,0}},
    {{0,0}, {0,0}, {0,0}, {2,0}}
  }
};


// todo pass projector
template <typename Float>
void multiplySpinorByDiracProjector(Float *res, int projIdx, Float *spinorIn) {
  for (uint i=0; i<4*3*2; i++) res[i] = 0.0;

  for (int s = 0; s < 4; s++) {
    for (int t = 0; t < 4; t++) {
      Float projRe = projector[projIdx][s][t][0];
      Float projIm = projector[projIdx][s][t][1];
      
      for (int m = 0; m < 3; m++) {
	Float spinorRe = spinorIn[t*(3*2) + m*(2) + 0];
	Float spinorIm = spinorIn[t*(3*2) + m*(2) + 1];
	res[s*(3*2) + m*(2) + 0] += projRe*spinorRe - projIm*spinorIm;
	res[s*(3*2) + m*(2) + 1] += projRe*spinorIm + projIm*spinorRe;
      }
    }
  }
}



// dslashReference_4d()
//J  This is just the 4d wilson dslash of quda code, with a
//J  few small changes to take into account that the spinors
//J  are 5d and the gauge fields are 4d.
//
// if oddBit is zero: calculate odd parity spinor elements (using even parity spinor)
// if oddBit is one:  calculate even parity spinor elements
//
// if daggerBit is zero: perform ordinary dslash operator
// if daggerBit is one:  perform hermitian conjugate of dslash
//
//An "ok" will only be granted once check2.tex is deemed complete,
//since the logic in this function is important and nontrivial.
template <typename sFloat, typename gFloat>
void dslashReference_4d(sFloat *res, gFloat **gaugeFull, sFloat *spinorField, 
                int oddBit, int daggerBit) {
  
  // Initialize the return half-spinor to zero.  Note that it is a
  // 5d spinor, hence the use of Nh_5d.
  for (uint i=0; i<Nh_5d*4*3*2; i++) res[i] = 0.0;
  
  // Some pointers that we use to march through arrays.
  gFloat *gaugeEven[4], *gaugeOdd[4];
  // Initialize to beginning of even and odd parts of
  // gauge array.
  for (int dir = 0; dir < 4; dir++) {  
    gaugeEven[dir] = gaugeFull[dir];
    // Note the use of Nh here, since the gauge fields
    // are 4-dim'l.
    gaugeOdd[dir]  = gaugeFull[dir]+Nh*gaugeSiteSize;
  }
  int sp_idx,oddBit_gge;
  for (int xs=0;xs<Ls;xs++) {
    for (int gge_idx = 0; gge_idx < Nh; gge_idx++) {
      for (int dir = 0; dir < 8; dir++) {
        sp_idx=gge_idx+Nh*xs;
        // Here is a function call to study.  It is defined near
        // Line 90 of this file.
        // Here we have to switch oddBit depending on the value of xs.  E.g., suppose
        // xs=1.  Then the odd spinor site x1=x2=x3=x4=0 wants the even gauge array
        // element 0, so that we get U_\mu(0).
        if ((xs % 2) == 0) oddBit_gge=oddBit;
        else oddBit_gge= (oddBit+1) % 2;
        gFloat *gauge = gaugeLink(gge_idx, dir, oddBit_gge, gaugeEven, gaugeOdd);
        
        // Even though we're doing the 4d part of the dslash, we need
        // to use a 5d neighbor function, to get the offsets right.
        sFloat *spinor = spinorNeighbor_5d(sp_idx, dir, oddBit, spinorField);
      
        sFloat projectedSpinor[4*3*2], gaugedSpinor[4*3*2];
        int projIdx = 2*(dir/2)+(dir+daggerBit)%2;
        multiplySpinorByDiracProjector(projectedSpinor, projIdx, spinor);
      
        for (int s = 0; s < 4; s++) {
	        if (dir % 2 == 0) {
        	  su3Mul(&gaugedSpinor[s*(3*2)], gauge, &projectedSpinor[s*(3*2)]);
#ifdef DBUG_VERBOSE            
            cout << "spinor:" << endl;
            printSpinorElement(&projectedSpinor[s*(3*2)],0,QUDA_DOUBLE_PRECISION);
            cout << "gauge:" << endl;
#endif
          } else {
        	  su3Tmul(&gaugedSpinor[s*(3*2)], gauge, &projectedSpinor[s*(3*2)]);
          }
        }
      
        sum(&res[sp_idx*(4*3*2)], &res[sp_idx*(4*3*2)], gaugedSpinor, 4*3*2);
      }
    }
  }
}

template <typename sFloat>
void dslashReference_5th(sFloat *res, sFloat *spinorField, 
                int oddBit, int daggerBit, sFloat mferm) {
  for (int i = 0; i < Nh_5d; i++) {
    for (int dir = 8; dir < 10; dir++) {
      // Calls for an extension of the original function.
      // 8 is forward hop, which wants P_+, 9 is backward hop,
      // which wants P_-.  Dagger reverses these.
      sFloat *spinor = spinorNeighbor_5d(i, dir, oddBit, spinorField);
      sFloat projectedSpinor[4*3*2];
      int projIdx = 2*(dir/2)+(dir+daggerBit)%2;
      multiplySpinorByDiracProjector(projectedSpinor, projIdx, spinor);
      //J  Need a conditional here for s=0 and s=Ls-1.
      int X = fullLatticeIndex_5d(i, oddBit);
      int xs = X/(L4*L3*L2*L1);
      if ( (xs == 0 && dir == 9) || (xs == Ls-1 && dir == 8) ) {
        product(projectedSpinor,(sFloat)(-mferm),projectedSpinor,4*3*2);
      } 
      sum(&res[i*(4*3*2)], &res[i*(4*3*2)], projectedSpinor, 4*3*2);
    }
  }
}

// Recall that dslash is only the off-diagonal parts, so m0_dwf is not needed.
//
void dslash_reference_5d(void *res, void **gaugeFull, void *spinorField, 
                int oddBit, int daggerBit, 
		      Precision sPrecision, Precision gPrecision, double mferm) {
  
  if (sPrecision == QUDA_DOUBLE_PRECISION)  {
    if (gPrecision == QUDA_DOUBLE_PRECISION) {
      // Do the 4d part, which hasn't changed.
      printf("doing 4d part\n"); fflush(stdout);
      dslashReference_4d<double,double>((double*)res, (double**)gaugeFull,
                      (double*)spinorField, oddBit, daggerBit);
      // Now add in the 5th dim.
      printf("doing 5th dimen. part\n"); fflush(stdout);
      dslashReference_5th<double>((double*)res, (double*)spinorField, 
                      oddBit, daggerBit, mferm);
    } else {
      dslashReference_4d<double,float>((double*)res, (float**)gaugeFull, (double*)spinorField, oddBit, daggerBit);
      dslashReference_5th<double>((double*)res, (double*)spinorField, oddBit, daggerBit, mferm);
    }
  } else {
    // Single-precision spinor.
    if (gPrecision == QUDA_DOUBLE_PRECISION) {
      dslashReference_4d<float,double>((float*)res, (double**)gaugeFull, (float*)spinorField, oddBit, daggerBit);
      dslashReference_5th<float>((float*)res, (float*)spinorField, oddBit, daggerBit, mferm);
    } else {
      // Do the 4d part, which hasn't changed.
      printf("CPU reference:  doing 4d part all single precision\n"); fflush(stdout);
      dslashReference_4d<float,float>((float*)res, (float**)gaugeFull, (float*)spinorField, oddBit, daggerBit);
      // Now add in the 5th dim.
      printf("CPU reference:  doing 5th dimen. part all single precision\n"); fflush(stdout);
      dslashReference_5th<float>((float*)res, (float*)spinorField, oddBit, daggerBit, mferm);
    }
  }
}


template <typename sFloat, typename gFloat>
void Mat(sFloat *out, gFloat **gauge, sFloat *in, sFloat kappa, sFloat mferm) {
  sFloat *inEven = in;
  sFloat *inOdd  = in + Nh_5d*spinorSiteSize;
  sFloat *outEven = out;
  sFloat *outOdd = out + Nh_5d*spinorSiteSize;
  
  // full dslash operator
  dslashReference_4d(outOdd, gauge, inEven, 1, 0);
  dslashReference_5th(outOdd, inEven, 1, 0, mferm);
  dslashReference_4d(outEven, gauge, inOdd, 0, 0);
  dslashReference_5th(outEven, inOdd, 0, 0, mferm);
  
  // lastly apply the kappa term
  xpay(in, -kappa, out, N_5d*spinorSiteSize);
}

template <typename sFloat, typename gFloat>
void MatDag(sFloat *out, gFloat **gauge, sFloat *in, sFloat kappa, sFloat mferm) {
  sFloat *inEven = in;
  sFloat *inOdd  = in + Nh_5d*spinorSiteSize;
  sFloat *outEven = out;
  sFloat *outOdd = out + Nh_5d*spinorSiteSize;
  
  // full dslash operator
  dslashReference_4d(outOdd, gauge, inEven, 1, 1);
  dslashReference_5th(outOdd, inEven, 1, 1, mferm);
  dslashReference_4d(outEven, gauge, inOdd, 0, 1);
  dslashReference_5th(outEven, inOdd, 0, 1, mferm);
  
  // lastly apply the kappa term
  xpay(in, -kappa, out, N_5d*spinorSiteSize);
}

void mat(void *out, void **gauge, void *in, double kappa, int dagger_bit, 
	 Precision sPrecision, Precision gPrecision, double mferm) {
  if (!dagger_bit) {
    if (sPrecision == QUDA_DOUBLE_PRECISION)
      if (gPrecision == QUDA_DOUBLE_PRECISION) Mat((double*)out, (double**)gauge, (double*)in, (double)kappa,
                      (double)mferm);
      else Mat((double*)out, (float**)gauge, (double*)in, (double)kappa, (double)mferm);
    else
      if (gPrecision == QUDA_DOUBLE_PRECISION) Mat((float*)out, (double**)gauge, (float*)in, (float)kappa,
                      (float)mferm);
      else Mat((float*)out, (float**)gauge, (float*)in, (float)kappa, (float)mferm);
  } else {
    if (sPrecision == QUDA_DOUBLE_PRECISION)
      if (gPrecision == QUDA_DOUBLE_PRECISION) MatDag((double*)out, (double**)gauge, (double*)in, (double)kappa,
                      (double)mferm);
      else MatDag((float*)out, (double**)gauge, (float*)in, (float)kappa, (float)mferm);
    else
      if (gPrecision == QUDA_DOUBLE_PRECISION) MatDag((float*)out, (double**)gauge, (float*)in, (float)kappa,
                      (float)mferm);
      else MatDag((float*)out, (float**)gauge, (float*)in, (float)kappa, (float)mferm);
  }
}

// Apply the even-odd preconditioned Dirac operator
template <typename sFloat, typename gFloat>
void MatPC(sFloat *outEven, gFloat **gauge, sFloat *inEven, sFloat kappa,
	   MatPCType matpc_type, sFloat mferm) {
  
  sFloat *tmp = (sFloat*)malloc(Nh_5d*spinorSiteSize*sizeof(sFloat));
    
  // full dslash operator
  if (matpc_type == QUDA_MATPC_EVEN_EVEN) {
    dslashReference_4d(tmp, gauge, inEven, 1, 0);
    dslashReference_5th(tmp, inEven, 1, 0, mferm);
    dslashReference_4d(outEven, gauge, tmp, 0, 0);
    dslashReference_5th(outEven, tmp, 0, 0, mferm);
  } else {
    dslashReference_4d(tmp, gauge, inEven, 0, 0);
    dslashReference_5th(tmp, inEven, 0, 0, mferm);
    dslashReference_4d(outEven, gauge, tmp, 1, 0);
    dslashReference_5th(outEven, tmp, 1, 0, mferm);
  }    
  
  // lastly apply the kappa term
  sFloat kappa2 = -kappa*kappa;
  xpay(inEven, kappa2, outEven, Nh_5d*spinorSiteSize);
  free(tmp);
}

// Apply the even-odd preconditioned Dirac operator
template <typename sFloat, typename gFloat>
void MatPCDag(sFloat *outEven, gFloat **gauge, sFloat *inEven, sFloat kappa, 
	      MatPCType matpc_type, sFloat mferm) {
  
  sFloat *tmp = (sFloat*)malloc(Nh_5d*spinorSiteSize*sizeof(sFloat));    
  
  // full dslash operator
  if (matpc_type == QUDA_MATPC_EVEN_EVEN) {
    dslashReference_4d(tmp, gauge, inEven, 1, 1);
    dslashReference_5th(tmp, inEven, 1, 1, mferm);
    dslashReference_4d(outEven, gauge, tmp, 0, 1);
    dslashReference_5th(outEven, tmp, 0, 1, mferm);
  } else {
    dslashReference_4d(tmp, gauge, inEven, 0, 1);
    dslashReference_5th(tmp, inEven, 0, 1, mferm);
    dslashReference_4d(outEven, gauge, tmp, 1, 1);
    dslashReference_5th(outEven, tmp, 1, 1, mferm);
  }
  
  sFloat kappa2 = -kappa*kappa;
  xpay(inEven, kappa2, outEven, Nh_5d*spinorSiteSize);
  free(tmp);
}

void matpc(void *outEven, void **gauge, void *inEven, double kappa, 
	   MatPCType matpc_type, int dagger_bit, Precision sPrecision, Precision gPrecision,
     double mferm) {
  if (!dagger_bit) {
    if (sPrecision == QUDA_DOUBLE_PRECISION)
      if (gPrecision == QUDA_DOUBLE_PRECISION) 
	MatPC((double*)outEven, (double**)gauge, (double*)inEven, (double)kappa, matpc_type, (double)mferm);
      else
	MatPC((double*)outEven, (float**)gauge, (double*)inEven, (double)kappa, matpc_type, (double)mferm);
    else
      if (gPrecision == QUDA_DOUBLE_PRECISION) 
	MatPC((float*)outEven, (double**)gauge, (float*)inEven, (float)kappa, matpc_type, (float)mferm);
      else
	MatPC((float*)outEven, (float**)gauge, (float*)inEven, (float)kappa, matpc_type, (float)mferm);
  } else {
    if (sPrecision == QUDA_DOUBLE_PRECISION)
      if (gPrecision == QUDA_DOUBLE_PRECISION) 
	MatPCDag((double*)outEven, (double**)gauge, (double*)inEven, (double)kappa, matpc_type, (double)mferm);
      else
	MatPCDag((double*)outEven, (float**)gauge, (double*)inEven, (double)kappa, matpc_type, (double)mferm);
    else
      if (gPrecision == QUDA_DOUBLE_PRECISION) 
	MatPCDag((float*)outEven, (double**)gauge, (float*)inEven, (float)kappa, matpc_type, (float)mferm);
      else
	MatPCDag((float*)outEven, (float**)gauge, (float*)inEven, (float)kappa, matpc_type, (float)mferm);
  }
}


template <typename sFloat, typename gFloat> 
void MatDagMat(sFloat *out, gFloat **gauge, sFloat *in, sFloat kappa, sFloat mferm) 
{
  // Allocate a full spinor.        
  sFloat *tmp = (sFloat*)malloc(N_5d*spinorSiteSize*sizeof(sFloat));
  // Call templates above.
  Mat(tmp, gauge, in, kappa, mferm);
  MatDag(out, gauge, tmp, kappa, mferm);
  free(tmp);
}

template <typename sFloat, typename gFloat> 
void MatPCDagMatPC(sFloat *out, gFloat **gauge, sFloat *in, sFloat kappa, 
		   MatPCType matpc_type, sFloat mferm)
{
  
  // Allocate half spinor
  sFloat *tmp = (sFloat*)malloc(Nh_5d*spinorSiteSize*sizeof(sFloat));
  // Apply the PC templates above
  MatPC(tmp, gauge, in, kappa, matpc_type, mferm);
  MatPCDag(out, gauge, tmp, kappa, matpc_type, mferm);
  free(tmp);
}

// Wrapper to templates that handles different precisions.
void matdagmat(void *out, void **gauge, void *in, double kappa,
	 Precision sPrecision, Precision gPrecision, double mferm) 
{
  if (sPrecision == QUDA_DOUBLE_PRECISION) {
    if (gPrecision == QUDA_DOUBLE_PRECISION) 
      MatDagMat((double*)out, (double**)gauge, (double*)in, (double)kappa,
          (double)mferm);
    else 
      MatDagMat((double*)out, (float**)gauge, (double*)in, (double)kappa, (double)mferm);
  } else {
    if (gPrecision == QUDA_DOUBLE_PRECISION) 
      MatDagMat((float*)out, (double**)gauge, (float*)in, (float)kappa,
          (float)mferm);
    else 
      MatDagMat((float*)out, (float**)gauge, (float*)in, (float)kappa, (float)mferm);
  }
}

// Wrapper to templates that handles different precisions.
void matpcdagmatpc(void *out, void **gauge, void *in, double kappa,
	 Precision sPrecision, Precision gPrecision, double mferm, MatPCType matpc_type) 
{
  if (sPrecision == QUDA_DOUBLE_PRECISION) {
    if (gPrecision == QUDA_DOUBLE_PRECISION) 
      MatPCDagMatPC((double*)out, (double**)gauge, (double*)in, (double)kappa,
        matpc_type, (double)mferm);
    else 
      MatPCDagMatPC((double*)out, (float**)gauge, (double*)in, (double)kappa,
                      matpc_type, (double)mferm);
  } else {
    if (gPrecision == QUDA_DOUBLE_PRECISION) 
      MatPCDagMatPC((float*)out, (double**)gauge, (float*)in, (float)kappa,
        matpc_type, (float)mferm);
    else 
      MatPCDagMatPC((float*)out, (float**)gauge, (float*)in, (float)kappa, 
                      matpc_type, (float)mferm);
  }
}

