// wilson_dslash_def.h - Dslash kernel definitions

// There are currently 72 different variants of the Wilson Dslash
// kernel, each one characterized by a set of 5 options, where each
// option can take one of several values (2*3*2*2*3 = 72).  This file
// is structured so that the C preprocessor loops through all 72
// variants (in a manner resembling a counter), sets the appropriate
// macros, and defines the corresponding functions.
//
// As an example of the function naming conventions, consider
//
// cloverDslash12DaggerXpayKernel(float4* out, ...).
//
// This is a clover Dslash^dagger kernel where the result is
// multiplied by "a" and summed with an input vector (Xpay), and the
// gauge matrix is reconstructed from 12 real numbers.  More
// generally, each function name is given by the concatenation of the
// following 4 fields, with "Kernel" at the end:
//
// DD_NAME_F = dslash, cloverDslash
// DD_RECON_F = 8, 12, 18
// DD_DAG_F = Dagger, [blank]
// DD_XPAY_F = Xpay, [blank]
//
// In addition, the kernels are templated on the precision of the
// fields (double, single, or half).

// initialize on first iteration

#ifndef DD_LOOP
#define DD_LOOP
#define DD_DAG 0
#define DD_XPAY 0
#define DD_RECON 0
#define DD_PREC 0
#define DD_CLOVER 0
#endif

// set options for current iteration

#if (DD_CLOVER==0) // no clover
#define DD_NAME_F dslash
#else              // clover
#define DSLASH_CLOVER
#define DD_NAME_F cloverDslash
#endif

#if (DD_DAG==0) // no dagger
#define DD_DAG_F
#else           // dagger
#define DD_DAG_F Dagger
#endif

#if (DD_XPAY==0) // no xpay 
#define DD_XPAY_F 
#define DD_PARAM5 const DslashParam param
#else            // xpay
#define DSLASH_XPAY
#define DD_XPAY_F Xpay
#if (DD_PREC == 0)
#define DD_PARAM5 const DslashParam param, const double2 *x, const float *xNorm, const double a
#elif (DD_PREC == 1) 
#define DD_PARAM5 const DslashParam param, const float4 *x, const float *xNorm, const float a
#else
#define DD_PARAM5 const DslashParam param, const short4 *x, const float *xNorm, const float a
#endif
#endif

#if (DD_RECON==0) // reconstruct from 8 reals
#define DD_RECON_F 8
#if (DD_PREC==0)
#define DD_PARAM2 const double2 *gauge0, const double2 *gauge1
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_8_DOUBLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_8_DOUBLE
#elif (DD_PREC==1)
#define DD_PARAM2 const float4 *gauge0, const float4 *gauge1
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_8_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_8_SINGLE
#else
#define DD_PARAM2 const short4 *gauge0, const short4* gauge1
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_8_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_8_HALF
#endif
#elif (DD_RECON==1) // reconstruct from 12 reals
#define DD_RECON_F 12
#if (DD_PREC==0)
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_12_DOUBLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_12_DOUBLE
#define DD_PARAM2 const double2 *gauge0, const double2 *gauge1
#elif (DD_PREC==1)
#define DD_PARAM2 const float4 *gauge0, const float4 *gauge1
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_12_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_12_SINGLE
#else
#define DD_PARAM2 const short4 *gauge0, const short4 *gauge1
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_12_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_12_SINGLE
#endif
#else               // no reconstruct, load all components
#define DD_RECON_F 18
#define GAUGE_FLOAT2
#if (DD_PREC==0)
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_18_DOUBLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_18_DOUBLE
#define DD_PARAM2 const double2 *gauge0, const double2 *gauge1
#elif (DD_PREC==1)
#define DD_PARAM2 const float4 *gauge0, const float4 *gauge1 // FIXME for direct reading, really float2
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_18_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_18_SINGLE
#else
#define DD_PARAM2 const short4 *gauge0, const short4 *gauge1 // FIXME for direct reading, really short2
#define RECONSTRUCT_GAUGE_MATRIX RECONSTRUCT_MATRIX_18_SINGLE
#define READ_GAUGE_MATRIX READ_GAUGE_MATRIX_18_SINGLE
#endif
#endif

#if (DD_PREC==0) // double-precision fields

// double-precision gauge field
#define GAUGE0TEX gauge0TexDouble2
#define GAUGE1TEX gauge1TexDouble2
#define GAUGE_FLOAT2

// double-precision spinor fields
#define DD_PARAM1 double2* out, float *null1
#define DD_PARAM4 const double2* in, const float *null4
#define READ_SPINOR READ_SPINOR_DOUBLE
#define READ_SPINOR_UP READ_SPINOR_DOUBLE_UP
#define READ_SPINOR_DOWN READ_SPINOR_DOUBLE_DOWN
#define SPINORTEX spinorTexDouble
#define WRITE_SPINOR WRITE_SPINOR_DOUBLE2
#define SPINOR_DOUBLE
#if (DD_XPAY==1)
#define ACCUMTEX accumTexDouble
#define READ_ACCUM READ_ACCUM_DOUBLE
#endif

#define SPINOR_HOP 12

// double-precision clover field
#if (DD_CLOVER==0)
#define DD_PARAM3
#else
#define DD_PARAM3 const double2 *clover, const float *null3,
#endif
#define CLOVERTEX cloverTexDouble
#define READ_CLOVER READ_CLOVER_DOUBLE
#define CLOVER_DOUBLE

#elif (DD_PREC==1) // single-precision fields

// single-precision gauge field
#if (DD_RECON_F == 18)
#define GAUGE0TEX gauge0TexSingle2
#define GAUGE1TEX gauge1TexSingle2
#else
#define GAUGE0TEX gauge0TexSingle4
#define GAUGE1TEX gauge1TexSingle4
#endif

// single-precision spinor fields
#define DD_PARAM1 float4* out, float *null1
#define DD_PARAM4 const float4* in, const float *null4
#define READ_SPINOR READ_SPINOR_SINGLE
#define READ_SPINOR_UP READ_SPINOR_SINGLE_UP
#define READ_SPINOR_DOWN READ_SPINOR_SINGLE_DOWN
#define SPINORTEX spinorTexSingle
#define WRITE_SPINOR WRITE_SPINOR_FLOAT4
#if (DD_XPAY==1)
#define ACCUMTEX accumTexSingle
#define READ_ACCUM READ_ACCUM_SINGLE
#endif

#define SPINOR_HOP 6

// single-precision clover field
#if (DD_CLOVER==0)
#define DD_PARAM3
#else
#define DD_PARAM3 const float4 *clover, const float *null3,
#endif
#define CLOVERTEX cloverTexSingle
#define READ_CLOVER READ_CLOVER_SINGLE

#else             // half-precision fields

// half-precision gauge field
#if (DD_RECON_F == 18)
#define GAUGE0TEX gauge0TexHalf2
#define GAUGE1TEX gauge1TexHalf2
#else
#define GAUGE0TEX gauge0TexHalf4
#define GAUGE1TEX gauge1TexHalf4
#endif

// half-precision spinor fields
#define READ_SPINOR READ_SPINOR_HALF
#define READ_SPINOR_UP READ_SPINOR_HALF_UP
#define READ_SPINOR_DOWN READ_SPINOR_HALF_DOWN
#define SPINORTEX spinorTexHalf
#define DD_PARAM1 short4* out, float *outNorm
#define DD_PARAM4 const short4* in, const float *inNorm
#define WRITE_SPINOR WRITE_SPINOR_SHORT4
#if (DD_XPAY==1)
#define ACCUMTEX accumTexHalf
#define READ_ACCUM READ_ACCUM_HALF
#endif

#define SPINOR_HOP 6

// half-precision clover field
#if (DD_CLOVER==0)
#define DD_PARAM3 
#else
#define DD_PARAM3 const short4 *clover, const float *cloverNorm,
#endif
#define CLOVERTEX cloverTexHalf
#define READ_CLOVER READ_CLOVER_HALF

#endif

// only build double precision if supported
#if !(__CUDA_ARCH__ < 130 && DD_PREC == 0) 

#define DD_CONCAT(n,r,d,x) n ## r ## d ## x ## Kernel
#define DD_FUNC(n,r,d,x) DD_CONCAT(n,r,d,x)

// define the kernel

__global__ void	DD_FUNC(DD_NAME_F, DD_RECON_F, DD_DAG_F, DD_XPAY_F)
  (DD_PARAM1, DD_PARAM2, DD_PARAM3 DD_PARAM4, DD_PARAM5) {

#ifdef GPU_WILSON_DIRAC
#if DD_DAG
#include "wilson_dslash_dagger_core.h"
#else
#include "wilson_dslash_core.h"
#endif
#endif

}

#endif

// clean up

#undef DD_NAME_F
#undef DD_RECON_F
#undef DD_DAG_F
#undef DD_XPAY_F
#undef DD_PARAM1
#undef DD_PARAM2
#undef DD_PARAM3
#undef DD_PARAM4
#undef DD_PARAM5
#undef DD_CONCAT
#undef DD_FUNC

#undef DSLASH_XPAY
#undef READ_GAUGE_MATRIX
#undef RECONSTRUCT_GAUGE_MATRIX
#undef GAUGE0TEX
#undef GAUGE1TEX
#undef READ_SPINOR
#undef READ_SPINOR_UP
#undef READ_SPINOR_DOWN
#undef SPINORTEX
#undef WRITE_SPINOR
#undef ACCUMTEX
#undef READ_ACCUM
#undef CLOVERTEX
#undef READ_CLOVER
#undef DSLASH_CLOVER
#undef GAUGE_FLOAT2
#undef SPINOR_DOUBLE
#undef CLOVER_DOUBLE
#undef SPINOR_HOP

// prepare next set of options, or clean up after final iteration

#if (DD_DAG==0)
#undef DD_DAG
#define DD_DAG 1
#else
#undef DD_DAG
#define DD_DAG 0

#if (DD_XPAY==0)
#undef DD_XPAY
#define DD_XPAY 1
#else
#undef DD_XPAY
#define DD_XPAY 0

#if (DD_RECON==0)
#undef DD_RECON
#define DD_RECON 1
#elif (DD_RECON==1)
#undef DD_RECON
#define DD_RECON 2
#else
#undef DD_RECON
#define DD_RECON 0

#if (DD_PREC==0)
#undef DD_PREC
#define DD_PREC 1
#elif (DD_PREC==1)
#undef DD_PREC
#define DD_PREC 2
#else
#undef DD_PREC
#define DD_PREC 0

#if (DD_CLOVER==0)
#undef DD_CLOVER
#define DD_CLOVER 1

#else

#undef DD_LOOP
#undef DD_DAG
#undef DD_XPAY
#undef DD_RECON
#undef DD_PREC
#undef DD_CLOVER

#endif // DD_CLOVER
#endif // DD_PREC
#endif // DD_RECON
#endif // DD_XPAY
#endif // DD_DAG

#ifdef DD_LOOP
#include "wilson_dslash_def.h"
#endif
