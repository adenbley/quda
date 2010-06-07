#ifndef _DSLASH_QUDA_H
#define _DSLASH_QUDA_H

#include <quda_internal.h>
#include <color_spinor_field.h>

#ifdef __cplusplus
extern "C" {
#endif

  extern int initDslash;

  int dslashCudaSharedBytes(QudaPrecision spinor_prec, int blockDim);

  void initDslashConstants(FullGauge gauge, int sp_stride, int cl_stride);
  void initStaggeredDslashConstants(FullGauge fatgauge, FullGauge longgauge);

  // plain wilson
  
    void dslashCuda(void *out, void *outNorm, const FullGauge gauge, const void *in, 
		    const void *inNorm, const int parity, const int dagger, 
		  const void *x, const void *xNorm, const double k,
		    const int volume, const int length, const QudaPrecision precision);
    
    // clover dslash

    void cloverDslashCuda(void *out, void *outNorm, const FullGauge gauge, 
			const FullClover cloverInv, const void *in, const void *inNorm,
			  const int oddBit, const int daggerBit, const void *x, const void *xNorm,
			  const double k, const int volume, const int length, const QudaPrecision precision);
    
    // solo clover term
  void cloverCuda(void *out, void *outNorm, const FullGauge gauge, const FullClover clover, 
		  const void *in, const void *inNorm, const int oddBit, const int volume, 
		  const int length, const QudaPrecision precision);

    //staggered
  void staggeredDslashCuda(void *out, void *outNorm, const FullGauge fatGauge, FullGauge longGauge,
			   cudaColorSpinorField *in,
			   const int parity, const int dagger, 
			   const void *x, const void *xNorm, const double k,
			   const int volume, const int length, const int ghost_length, const QudaPrecision precision);
    
    


#ifdef __cplusplus
}
#endif

#endif // _DLASH_QUDA_H
