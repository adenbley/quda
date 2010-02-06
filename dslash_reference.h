#include <blas_reference.h>

#ifndef _QUDA_DSLASH_REF_H
#define _QUDA_DSLASH_REF_H

#ifdef __cplusplus
extern "C" {
#endif

  // -- dslash_reference.cpp
  
  void dslash_reference_5d(void *res, void **gauge, void *spinorField, 
			int oddBit, int daggerBit, Precision sPrecision, Precision gPrecision, double mferm);
  
  void mat(void *out, void **gauge, void *in, double kappa, int daggerBit, Precision sPrecision, Precision gPrecision,
                  double mferm);

  void matpc(void *out, void **gauge, void *in, double kappa, MatPCType matpc_type, 
	     int daggerBit, Precision sPrecision, Precision gPrecision, double mferm);

// Wrapper to templates that handles different precisions.
void matdagmat(void *out, void **gauge, void *in, double kappa,
	 Precision sPrecision, Precision gPrecision, double mferm);

// Wrapper to templates that handles different precisions.
void matpcdagmatpc(void *out, void **gauge, void *in, double kappa,
	 Precision sPrecision, Precision gPrecision, double mferm, MatPCType matpc_type);
  
#ifdef __cplusplus
}
#endif

#endif // _QUDA_DLASH_REF_H
