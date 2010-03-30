#ifndef _QUDA_HW_H
#define _QUDA_HW_H

#include <enum_quda.h>
#include <dslash_quda.h>

#ifdef __cplusplus
extern "C" {
#endif

    FullHw createHwQuda(int* X, Precision precision);
    void loadHwToGPU(FullHw ret, void* hw, Precision cpu_prec);
    
    void freeHwQuda(FullHw hw);
    
#ifdef __cplusplus
}
#endif

#endif // _QUDA_HW_H
