#ifndef _QUDA_BLAS_REF_H
#define _QUDA_BLAS_REF_H

#include <quda.h>

#ifdef __cplusplus
extern "C" {
#endif
  
  // ---------- blas_reference.cpp ----------
    
    double norm_2(void *vector, int len, Precision precision);
    void mxpy(void *x, void *y, int len, Precision precision);
    void ax(double a, void *x, int len, Precision precision);
    double relative_norm_2(void* p, void *q, int len, Precision precision) ;
    /*  void zero(float* a, int cnt);
      void copy(float* a, float *b, int len);*/
  
  /*void axpbyCuda(float a, float *x, float b, float *y, int len);
    void axpy(float a, float *x, float *y, int len);*/
  //void xpay(float *x, float a, float *y, int len);
  
  /*float reDotProduct(float *v1, float *v2, int len);
  float imDotProduct(float *v1, float *v2, int len);
  double normD(float *vector, int len);
  double reDotProductD(float *v1, float *v2, int len);
  double imDotProductD(float *v1, float *v2, int len);*/

#ifdef __cplusplus
}
#endif

#endif // _QUDA_BLAS_REF_H
