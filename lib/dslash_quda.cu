
#include <stdlib.h>
#include <stdio.h>

#include <color_spinor_field.h>

#define BLOCK_DIM 64

//these are access control for staggered action
#if (__CUDA_ARCH__ >= 200)
//#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
#define DIRECT_ACCESS_SPINOR
#else
#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
//#define DIRECT_ACCESS_SPINOR
#endif

#include <quda_internal.h>
#include <dslash_quda.h>

struct DslashParam {
  int tOffset; // offset into the T dimension (multi gpu only)
  int tMul;    // spatial volume distance between the T faces being updated (multi gpu only)
  int threads; // the desired number of active threads
  int parity;  // Even-Odd or Odd-Even
  int ghostDim[QUDA_MAX_DIM];
  int ghostOffset[QUDA_MAX_DIM];
};

DslashParam dslashParam;

// these are set in initDslashConst
int Vspatial;
#ifdef MULTI_GPU
static const int Nstream = 3;
#else
static const int Nstream = 1;
#endif
static cudaStream_t streams[Nstream];

FaceBuffer *face;
cudaColorSpinorField *inSpinor;

#include <dslash_textures.h>
#include <dslash_constants.h>

#include <staggered_dslash_def.h> // staggered Dslash kernels
#include <wilson_dslash_def.h>    // Wilson Dslash kernels (including clover)
#include <dw_dslash_def.h>        // Domain Wall kernels
#include <tm_dslash_def.h>        // Twisted Mass kernels
#include <dslash_core/tm_core.h>  // solo twisted mass kernel
#include <clover_def.h>           // kernels for applying the clover term alone

#ifndef SHARED_FLOATS_PER_THREAD
#define SHARED_FLOATS_PER_THREAD 0
#endif

#include <blas_quda.h>
#include <face_quda.h>


// dslashTuning = QUDA_TUNE_YES turns off error checking
static QudaTune dslashTuning = QUDA_TUNE_NO;

void setDslashTuning(QudaTune tune)
{
  dslashTuning = tune;
}

__global__ void dummyKernel() {
  // do nothing
}

void initCache() {

#if (__CUDA_ARCH__ >= 200)

  static int firsttime = 1;
  if (firsttime){	
    cudaFuncSetCacheConfig(dummyKernel, cudaFuncCachePreferL1);
    dummyKernel<<<1,1>>>();
    firsttime=0;
  }

#endif

}

void setFace(const FaceBuffer &Face) {
  face = (FaceBuffer*)&Face; // nasty
}

template <int spinorN, typename spinorFloat, typename gaugeFloat>
void dslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat *gauge0, const gaugeFloat *gauge1, 
		const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
		const int dagger, const spinorFloat *x, const float *xNorm, const double &a, 
		const int volume, const size_t bytes, const size_t norm_bytes, cudaStream_t &stream,
		const int shared_bytes, const dim3 blockDim) 
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	dslash18Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      } else {
	dslash18DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	dslash12Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      } else {
	dslash12DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      }
    } else {
      if (!dagger) {
	dslash8Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      } else {
	dslash8DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam);
      }
    }
  } else { // doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	dslash18XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	dslash18DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	dslash12XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	dslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	dslash8XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	dslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, x, xNorm, a);
      }
    }
  }
 
}

template <int spinorN, typename spinorFloat, typename gaugeFloat>
void dslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat *gauge0, const gaugeFloat *gauge1, 
		const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
		const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
		const double &a, const int volume, const size_t bytes, const size_t norm_bytes,
		const dim3 block, const dim3 blockFace) {

  int shared_bytes = block.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);

  dslashParam.parity = parity;

#ifndef MULTI_GPU
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = volume;

  dslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
		      dagger, x, xNorm, a, volume, bytes, norm_bytes, streams[0], shared_bytes, block);
#else

  // Gather from source spinor
  face->exchangeFacesStart(*inSpinor, 1-parity, dagger, streams);
  
#ifdef OVERLAP_COMMS // do body
  dslashParam.tOffset = 1;
  dslashParam.tMul = 1;
  dslashParam.threads = volume - 2*Vspatial;
  dslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
		      dagger, x, xNorm, a, volume, bytes, norm_bytes, streams[Nstream-1], shared_bytes, block);    
#endif // OVERLAP_COMMS

  // Finish gather and start comms
  face->exchangeFacesComms();

  // Wait for comms to finish, and scatter into the end zone
  face->exchangeFacesWait(*inSpinor, dagger);

  dslashParam.tOffset = 0;
#ifdef OVERLAP_COMMS // do faces
  dslashParam.tMul = volume/Vspatial - 1; // hacky way to get Nt
  dslashParam.threads = 2*Vspatial;
#else // do all
  dslashParam.tMul = 1;
  dslashParam.threads = volume;
#endif // OVERLAP_COMMS
  shared_bytes = blockFace.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);
  dslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
		      dagger, x, xNorm, a, volume, bytes, norm_bytes, streams[Nstream-2], shared_bytes, blockFace);    

#endif // MULTI_GPU

  // texture unbinding is not asynchronous so we don't need to cudaThreadSynchonize()
  unbindSpinorTex<spinorN>(in, inNorm, x, xNorm); 
}

// Wilson wrappers
void dslashCuda(cudaColorSpinorField *out, const FullGauge gauge, const cudaColorSpinorField *in,
		const int parity, const int dagger, const cudaColorSpinorField *x,
		const double &k, const dim3 &block, const dim3 &blockFace) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_WILSON_DIRAC
  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  void *xv = (x ? x->v : 0);
  void *xn = (x ? x->norm : 0);

  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslashCuda<2>((double2*)out->v, (float*)out->norm, (double2*)gauge0, (double2*)gauge1, 
		  gauge.reconstruct, (double2*)in->v, (float*)in->norm, parity, dagger, 
		  (double2*)xv, (float*)xn, k, in->volume, in->bytes, in->norm_bytes, block, blockFace);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    dslashCuda<4>((float4*)out->v, (float*)out->norm, (float4*)gauge0, (float4*)gauge1,
    		  gauge.reconstruct, (float4*)in->v, (float*)in->norm, parity, dagger, 
		  (float4*)xv, (float*)xn, k, in->volume, in->bytes, in->norm_bytes, block, blockFace);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    dslashCuda<4>((short4*)out->v, (float*)out->norm, (short4*)gauge0, (short4*)gauge1,
		  gauge.reconstruct, (short4*)in->v, (float*)in->norm, parity, dagger, 
		  (short4*)xv, (float*)xn, k, in->volume, in->bytes, in->norm_bytes, block, blockFace);
  }
  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Wilson dslash has not been built");
#endif // GPU_WILSON_DIRAC

}

// FIXME: cloverCuda cannot be issued asynchronously because of texture unbinding and checkCudaError
template <int N, typename spinorFloat, typename cloverFloat>
void cloverCuda(spinorFloat *out, float *outNorm, const cloverFloat *clover,
		const float *cloverNorm, const spinorFloat *in, const float *inNorm, 
		const size_t bytes, const size_t norm_bytes, const dim3 blockDim)
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<N>(bytes, norm_bytes, in, inNorm);
  cloverKernel<<<gridDim, blockDim, shared_bytes>>> 
    (out, outNorm, clover, cloverNorm, in, inNorm, dslashParam);
  unbindSpinorTex<N>(in, inNorm);
}

void cloverCuda(cudaColorSpinorField *out, const FullGauge gauge, const FullClover clover, 
		const cudaColorSpinorField *in, const int parity, const dim3 &blockDim) {

  dslashParam.parity = parity;
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = in->volume;

#ifdef GPU_WILSON_DIRAC
  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(clover, parity, &cloverP, &cloverNormP);

  if (in->precision != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    cloverCuda<2>((double2*)out->v, (float*)out->norm, (double2*)cloverP, 
		  (float*)cloverNormP, (double2*)in->v, (float*)in->norm, 
		  in->bytes, in->norm_bytes, blockDim);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    cloverCuda<4>((float4*)out->v, (float*)out->norm, (float4*)cloverP, 
		  (float*)cloverNormP, (float4*)in->v, (float*)in->norm,
		  in->bytes, in->norm_bytes, blockDim);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    cloverCuda<4>((short4*)out->v, (float*)out->norm, (short4*)cloverP, 
		  (float*)cloverNormP, (short4*)in->v, (float*)in->norm, 
		  in->bytes, in->norm_bytes, blockDim);
  }
  unbindCloverTex(clover);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Clover dslash has not been built");
#endif

}

// Clover wrappers
template <int N, typename spinorFloat, typename cloverFloat, typename gaugeFloat>
void cloverDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat gauge0, 
		      const gaugeFloat gauge1, const QudaReconstructType reconstruct, 
		      const cloverFloat *clover, const float *cloverNorm, const spinorFloat *in, 
		      const float* inNorm, const int dagger, const spinorFloat *x, 
		      const float* xNorm, const double &a, const size_t bytes, 
		      const size_t norm_bytes, cudaStream_t &stream, const int shared_bytes, 
		      const dim3 blockDim) 
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  if (x==0) { // not xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	cloverDslash18Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      } else {
	cloverDslash18DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	cloverDslash12Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      } else {
	cloverDslash12DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      }
    } else {
      if (!dagger) {
	cloverDslash8Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 	
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      } else {
	cloverDslash8DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam);
      }
    }
  } else { // doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	cloverDslash18XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	cloverDslash18DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	cloverDslash12XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	cloverDslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      }
    } else {
      if (!dagger) {
	cloverDslash8XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 	
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      } else {
	cloverDslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, dslashParam, x, xNorm, a);
      }
    }
  }
}

template <int spinorN, typename spinorFloat, typename cloverFloat, typename gaugeFloat>
void cloverDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat *gauge0, const gaugeFloat *gauge1, 
		      const QudaReconstructType reconstruct, const cloverFloat *clover, const float *cloverNorm, 
		      const spinorFloat *in, const float *inNorm, const int parity, const int dagger, 
		      const spinorFloat *x, const float *xNorm, const double &a, const int volume, 
		      const size_t bytes, const size_t norm_bytes, const dim3 block, const dim3 blockFace) {

  int shared_bytes = block.x*SHARED_FLOATS_PER_THREAD*
    bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);

  dslashParam.parity = parity;

#ifndef MULTI_GPU
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = volume;

  cloverDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, clover, cloverNorm, in, inNorm, 
			    dagger, x, xNorm, a, bytes, norm_bytes, streams[0], shared_bytes, block);
#else

  // Gather from source spinor
  face->exchangeFacesStart(*inSpinor, 1-parity, dagger, streams);
  
#ifdef OVERLAP_COMMS // do body
  dslashParam.tOffset = 1;
  dslashParam.tMul = 1;
  dslashParam.threads = volume - 2*Vspatial;
  cloverDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, clover, cloverNorm, in, inNorm, 
			    dagger, x, xNorm, a, bytes, norm_bytes, streams[Nstream-1], shared_bytes,
			    block);
#endif // OVERLAP_COMMS

  // Finish gather and start comms
  face->exchangeFacesComms();

  // Wait for comms to finish, and scatter into the end zone
  face->exchangeFacesWait(*inSpinor, dagger);

  dslashParam.tOffset = 0;
#ifdef OVERLAP_COMMS // do faces
  dslashParam.tMul = volume/Vspatial - 1; // hacky way to get Nt
  dslashParam.threads = 2*Vspatial;
#else // do all
  dslashParam.tMul = 1;
  dslashParam.threads = volume;
#endif // OVERLAP_COMMS
  shared_bytes = blockFace.x*SHARED_FLOATS_PER_THREAD*
    bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);
  cloverDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, clover, cloverNorm, in, inNorm, 
			    dagger, x, xNorm, a, bytes, norm_bytes, streams[Nstream-2], shared_bytes,
			    blockFace);

#endif // MULTI_GPU

  // texture unbinding is not asynchronous so we don't need to cudaThreadSynchonize()
  unbindSpinorTex<spinorN>(in, inNorm, x, xNorm); 
}

void cloverDslashCuda(cudaColorSpinorField *out, const FullGauge gauge, const FullClover cloverInv,
		      const cudaColorSpinorField *in, const int parity, const int dagger, 
		      const cudaColorSpinorField *x, const double &a,
		      const dim3 &block, const dim3 &blockFace) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_WILSON_DIRAC
  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(cloverInv, parity, &cloverP, &cloverNormP);

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  if (in->precision != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  void *xv = x ? x->v : 0;
  void *xn = x ? x->norm : 0;

  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    cloverDslashCuda<2>((double2*)out->v, (float*)out->norm, (double2*)gauge0, (double2*)gauge1, 
			gauge.reconstruct, (double2*)cloverP, (float*)cloverNormP, (double2*)in->v,
			(float*)in->norm, parity, dagger, (double2*)xv, (float*)xn, a, 
			in->volume, in->bytes, in->norm_bytes, block, blockFace);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    cloverDslashCuda<4>((float4*)out->v, (float*)out->norm, (float4*)gauge0, (float4*)gauge1, 
			gauge.reconstruct, (float4*)cloverP, (float*)cloverNormP, (float4*)in->v, 
			(float*)in->norm, parity, dagger, (float4*)xv, (float*)xn, a, 
			in->volume, in->bytes, in->norm_bytes, block, blockFace);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    cloverDslashCuda<4>((short4*)out->v, (float*)out->norm, (short4*)gauge0, (short4*)gauge1, 
			gauge.reconstruct, (short4*)cloverP, (float*)cloverNormP, (short4*)in->v,
			(float*)in->norm, parity, dagger, (short4*)xv, (float*)xn, a, 
			in->volume, in->bytes, in->norm_bytes, block, blockFace);
  }

  unbindGaugeTex(gauge);
  unbindCloverTex(cloverInv);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Clover dslash has not been built");
#endif

}

// Domain wall wrappers
template <int N, typename spinorFloat, typename gaugeFloat>
void domainWallDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat gauge0, 
			  const gaugeFloat gauge1, const QudaReconstructType reconstruct, 
			  const spinorFloat *in, const float* inNorm, 
			  const int dagger, const spinorFloat *x, const float* xNorm, 
			  const double &m_f, const double &k2, const size_t bytes, 
			  const size_t norm_bytes, const dim3 blockDim)
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);
  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<N>(bytes, norm_bytes, in, inNorm, x, xNorm);

  if (x==0) { // not xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	domainWallDslash18Kernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      } else {
	domainWallDslash18DaggerKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	domainWallDslash12Kernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      } else {
	domainWallDslash12DaggerKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      }
    } else {
      if (!dagger) {
	domainWallDslash8Kernel <<<gridDim, blockDim, shared_bytes>>> 	
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      } else {
	domainWallDslash8DaggerKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f);
      }
    }
  } else { // doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	domainWallDslash18XpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      } else {
	domainWallDslash18DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	domainWallDslash12XpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      } else {
	domainWallDslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      }
    } else {
      if (!dagger) {
	domainWallDslash8XpayKernel <<<gridDim, blockDim, shared_bytes>>> 	
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      } else {
	domainWallDslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, m_f, x, xNorm, k2);
      }
    }
  }

  unbindSpinorTex<N>(in, inNorm, x, xNorm);
}

void domainWallDslashCuda(cudaColorSpinorField *out, const FullGauge gauge, 
			  const cudaColorSpinorField *in, const int parity, const int dagger, 
			  const cudaColorSpinorField *x, const double &m_f, const double &k2,
			  const dim3 &block, const dim3 &blockFace) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef MULTI_GPU
  errorQuda("Multi-GPU domain wall not implemented\n");
#endif

  dslashParam.parity = parity;
  dslashParam.threads = in->volume;

#ifdef GPU_DOMAIN_WALL_DIRAC
  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  void *xv = x ? x->v : 0;
  void *xn = x ? x->norm : 0;

  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    domainWallDslashCuda<2>((double2*)out->v, (float*)out->norm, (double2*)gauge0, (double2*)gauge1, 
			    gauge.reconstruct, (double2*)in->v, (float*)in->norm, dagger, 
			    (double2*)xv, (float*)xn, m_f, k2, in->bytes, in->norm_bytes, block);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    domainWallDslashCuda<4>((float4*)out->v, (float*)out->norm, (float4*)gauge0, (float4*)gauge1, 
			    gauge.reconstruct, (float4*)in->v, (float*)in->norm, dagger, 
			    (float4*)xv, (float*)xn, m_f, k2, in->bytes, in->norm_bytes, block);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    domainWallDslashCuda<4>((short4*)out->v, (float*)out->norm, (short4*)gauge0, (short4*)gauge1, 
			    gauge.reconstruct, (short4*)in->v, (float*)in->norm, dagger, 
			    (short4*)xv, (float*)xn, m_f, k2, in->bytes, in->norm_bytes, block);
  }

  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Domain wall dslash has not been built");
#endif

}


#define INTERIOR_KERNEL 0
#define EXTERIOR_KERNEL_X 1
#define EXTERIOR_KERNEL_Y 2
#define EXTERIOR_KERNEL_Z 3
#define EXTERIOR_KERNEL_T 4


void
initTLocation(int toffset, int tmul, int threads) 
{
  short2 tLocate = make_short2((short)toffset, (short)tmul);
  cudaMemcpyToSymbol("tLocate", &(tLocate), sizeof(short2));
  cudaMemcpyToSymbol("threads", &(threads), sizeof(threads));

}

template <int spinorN, typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
  void staggeredDslashCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
			   const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
			   const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
			   const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
			   const double &a, const int volume, const int* Vsh, const int* dims,
			   const int length, const int ghost_length, dim3 blockDim) {
    
  dim3 interiorGridDim( (dslashParam.threads + blockDim.x -1)/blockDim.x, 1, 1);
  dim3 exteriorGridDim[4]  = {
    dim3((6*Vsh[0] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[1] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[2] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[3] + blockDim.x -1)/blockDim.x, 1, 1)
  };
    
  int shared_bytes = blockDim.x*6*bindSpinorTex_mg<spinorN>(length, ghost_length, in, inNorm, x, xNorm); CUERR;

  initTLocation(0, INTERIOR_KERNEL, volume);  CUERR;

#ifdef MULTI_GPU
  // Gather from source spinor
  face->exchangeFacesStart(*inSpinor, 1-parity, dagger, streams);
#endif

  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12Kernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
      } else {
	staggeredDslash12DaggerKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>> 
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8){
      
      if (!dagger) {
	staggeredDslash8Kernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>> 
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
      } else {
	staggeredDslash8DaggerKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value(%d) in function %s\n", reconstruct, __FUNCTION__);
    }
  } else { // doing xpay
    
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12AxpyKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>> 
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      } else {
	staggeredDslash12DaggerAxpyKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	staggeredDslash8AxpyKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      } else {
	staggeredDslash8DaggerAxpyKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value in function %s\n", __FUNCTION__);	  
    }    
  }

#ifdef MULTI_GPU
  // Finish gather and start comms
  face->exchangeFacesComms();
  // Wait for comms to finish, and scatter into the end zone
  face->exchangeFacesWait(*inSpinor, dagger);

  int exterior_kernel_flag[4]={
    EXTERIOR_KERNEL_X, EXTERIOR_KERNEL_Y, EXTERIOR_KERNEL_Z, EXTERIOR_KERNEL_T
  };
  for(int i=0 ;i < 4;i++){
    initTLocation(dims[i]-6, exterior_kernel_flag[i] , 6*Vsh[i]);  
    if (x==0) { // not doing xpay
      if (reconstruct == QUDA_RECONSTRUCT_12) {
	if (!dagger) {
	  staggeredDslash12Kernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
	} else {
	  staggeredDslash12DaggerKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>> 
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
	}
      } else if (reconstruct == QUDA_RECONSTRUCT_8){
	
	if (!dagger) {
	  staggeredDslash8Kernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>> 
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
	} else {
	  staggeredDslash8DaggerKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam); CUERR;
	}
      }else{
	errorQuda("Invalid reconstruct value(%d) in function %s\n", reconstruct, __FUNCTION__);
      }
    } else { // doing xpay
      
      if (reconstruct == QUDA_RECONSTRUCT_12) {
	if (!dagger) {
	  staggeredDslash12AxpyKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>> 
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
	} else {
	  staggeredDslash12DaggerAxpyKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
	}
      } else if (reconstruct == QUDA_RECONSTRUCT_8) {
	if (!dagger) {
	  staggeredDslash8AxpyKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	    (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
	} else {
	  staggeredDslash8DaggerAxpyKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
	}
      }else{
	errorQuda("Invalid reconstruct value in function %s\n", __FUNCTION__);	  
      }    
    }
  }

#endif
}



//This function is a special case for 18(no) reconstruct long link
//The reason is to make the type match easier(e.g float2 instead of float4)

template <int spinorN, typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
  void staggeredDslashNoReconCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
				  const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
				  const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
				  const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
				  const double &a, const int volume, const int* Vsh, const int* dims,
				  const int length, const int ghost_length, dim3 blockDim) 
{
  dim3 interiorGridDim( (dslashParam.threads + blockDim.x -1)/blockDim.x, 1, 1);
  dim3 exteriorGridDim[4]={ 
    dim3((6*Vsh[0] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[1] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[2] + blockDim.x -1)/blockDim.x, 1, 1),
    dim3((6*Vsh[3] + blockDim.x -1)/blockDim.x, 1, 1)
  };
    
  
  int shared_bytes = blockDim.x*6*bindSpinorTex_mg<spinorN>(length, ghost_length, in, inNorm, x, xNorm);
  
  initTLocation(0, INTERIOR_KERNEL, volume);  

#ifdef MULTI_GPU
  // Gather from source spinor
  face->exchangeFacesStart(*inSpinor, 1-parity, dagger, streams);
#endif

  if (x==0) { // not doing xpay
    if (!dagger) {
      staggeredDslash18Kernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	(out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam);CUERR;
    } else {
      staggeredDslash18DaggerKernel <<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>> 
	(out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam);CUERR;
    }    
  } else { // doing xpay
    
    if (!dagger) {
      staggeredDslash18AxpyKernel<<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	(out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
    } else {
      staggeredDslash18DaggerAxpyKernel<<<interiorGridDim, blockDim, shared_bytes, streams[Nstream-1]>>>
	(out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
    }          
  }

#ifdef MULTI_GPU
  // Finish gather and start comms
  face->exchangeFacesComms();
  // Wait for comms to finish, and scatter into the end zone
  face->exchangeFacesWait(*inSpinor, dagger);

  int exterior_kernel_flag[4]={EXTERIOR_KERNEL_X, EXTERIOR_KERNEL_Y, EXTERIOR_KERNEL_Z, EXTERIOR_KERNEL_T};
  for(int i=0; i< 4; i++){
    initTLocation(dims[i] -6,exterior_kernel_flag[i] , 6*Vsh[i]);  
    if (x==0) { // not doing xpay
      if (!dagger) {
	staggeredDslash18Kernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	(out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam);CUERR;
      } else {
	staggeredDslash18DaggerKernel <<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>> 
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam);CUERR;
      }    
    } else { // doing xpay
      if (!dagger) {
	staggeredDslash18AxpyKernel<<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      } else {
	staggeredDslash18AxpyKernel<<<exteriorGridDim[i], blockDim, shared_bytes, streams[Nstream-2]>>>
	  (out, outNorm, fatGauge0, fatGauge1, longGauge0, longGauge1, in, inNorm, dslashParam, x, xNorm, a); CUERR;
      }          
    }     
  }
    
#endif
  
}


void staggeredDslashCuda(cudaColorSpinorField *out, const FullGauge fatGauge, 
			 const FullGauge longGauge, const cudaColorSpinorField *in,
			 const int parity, const int dagger, const cudaColorSpinorField *x,
			 const double &k, const dim3 &block, const dim3 &blockFace)
{
  
  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_STAGGERED_DIRAC

  dslashParam.parity = parity;
  dslashParam.threads = in->volume;
  for(int i=0;i < 4;i++){
    dslashParam.ghostDim[i] = in->ghostDim[i];
    dslashParam.ghostOffset[i] = in->ghostOffset[i];
  }
  void *fatGauge0, *fatGauge1;
  void* longGauge0, *longGauge1;
  bindFatGaugeTex(fatGauge, parity, &fatGauge0, &fatGauge1);
  bindLongGaugeTex(longGauge, parity, &longGauge0, &longGauge1);
    
  if (in->precision != fatGauge.precision || in->precision != longGauge.precision){
    errorQuda("Mixing gauge and spinor precision not supported"
	      "(precision=%d, fatlinkGauge.precision=%d, longGauge.precision=%d",
	      in->precision, fatGauge.precision, longGauge.precision);
  }
    
  int Vsh[] = {
    in->x[1]*in->x[2]*in->x[3]/2,
    in->x[0]*in->x[2]*in->x[3],
    in->x[0]*in->x[1]*in->x[3],
    in->x[0]*in->x[1]*in->x[2]};

  void *xv = x ? x->v : 0;
  void *xn = x ? x->norm : 0;

  if (in->precision == QUDA_DOUBLE_PRECISION) {
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((double2*)out->v, (float*)out->norm, (double2*)fatGauge0, (double2*)fatGauge1,       
				    (double2*)longGauge0, (double2*)longGauge1, longGauge.reconstruct, 
				    (double2*)in->v, (float*)in->norm, parity, dagger, 
				    (double2*)xv, (float*)x, k, in->volume, Vsh, 
				    in->x, in->length, in->ghost_length, block);
    }else{
      staggeredDslashCuda<2>((double2*)out->v, (float*)out->norm, (double2*)fatGauge0, (double2*)fatGauge1,
			     (double2*)longGauge0, (double2*)longGauge1, longGauge.reconstruct, 
			     (double2*)in->v, (float*)in->norm, parity, dagger, 
			     (double2*)xv, (float*)x, k, in->volume, Vsh, 
			     in->x, in->length, in->ghost_length, block);
    }
    
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((float2*)out->v, (float*)out->norm, (float2*)fatGauge0, (float2*)fatGauge1,
				    (float2*)longGauge0, (float2*)longGauge1, longGauge.reconstruct, 
				    (float2*)in->v, (float*)in->norm, parity, dagger, 
				    (float2*)xv, (float*)xn, k, in->volume, Vsh, 
				    in->x, in->length, in->ghost_length, block);
    }else{
      staggeredDslashCuda<2>((float2*)out->v, (float*)out->norm, (float2*)fatGauge0, (float2*)fatGauge1,
			     (float4*)longGauge0, (float4*)longGauge1, longGauge.reconstruct, 
			     (float2*)in->v, (float*)in->norm, parity, dagger, 
			     (float2*)xv, (float*)xn, k, in->volume, Vsh, 
			     in->x, in->length, in->ghost_length, block);
    }
  } else if (in->precision == QUDA_HALF_PRECISION) {	
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((short2*)out->v, (float*)out->norm, (short2*)fatGauge0, (short2*)fatGauge1,
				    (short2*)longGauge0, (short2*)longGauge1, longGauge.reconstruct, 
				    (short2*)in->v, (float*)in->norm, parity, dagger, 
				    (short2*)xv, (float*)xn, k, in->volume, Vsh, 
				    in->x, in->length, in->ghost_length, block);
    }else{
      staggeredDslashCuda<2>((short2*)out->v, (float*)out->norm, (short2*)fatGauge0, (short2*)fatGauge1,
			     (short4*)longGauge0, (short4*)longGauge1, longGauge.reconstruct, 
			     (short2*)in->v, (float*)in->norm, parity, dagger, 
			     (short2*)xv, (float*)xn, k, in->volume, Vsh, 
			     in->x, in->length, in->ghost_length, block);
    }
  }

  if (!dslashTuning) checkCudaError();
  
#else
  errorQuda("Staggered dslash has not been built");
#endif  
}





void setTwistParam(double &a, double &b, const double &kappa, const double &mu, 
		   const int dagger, const QudaTwistGamma5Type twist) {
  if (twist == QUDA_TWIST_GAMMA5_DIRECT) {
    a = 2.0 * kappa * mu;
    b = 1.0;
  } else if (twist == QUDA_TWIST_GAMMA5_INVERSE) {
    a = -2.0 * kappa * mu;
    b = 1.0 / (1.0 + a*a);
  } else {
    errorQuda("Twist type %d not defined\n", twist);
  }
  if (dagger) a *= -1.0;

}

// FIXME: twist kernel cannot be issued asynchronously because of texture unbinding
template <int N, typename spinorFloat>
void twistGamma5Cuda(spinorFloat *out, float *outNorm, const spinorFloat *in, 
		     const float *inNorm, const int dagger, const double &kappa, 
		     const double &mu, const size_t bytes, const size_t norm_bytes, 
		     const QudaTwistGamma5Type twist, dim3 blockDim)
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  double a=0.0, b=0.0;
  setTwistParam(a, b, kappa, mu, dagger, twist);

  bindSpinorTex<N>(bytes, norm_bytes, in, inNorm);
  twistGamma5Kernel<<<gridDim, blockDim, 0>>> (out, outNorm, a, b, dslashParam);
  unbindSpinorTex<N>(in, inNorm);
}

void twistGamma5Cuda(cudaColorSpinorField *out, const cudaColorSpinorField *in,
		     const int dagger, const double &kappa, const double &mu,
		     const QudaTwistGamma5Type twist, const dim3 &block) {

  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = in->Volume();

#ifdef GPU_TWISTED_MASS_DIRAC
  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    twistGamma5Cuda<2>((double2*)out->v, (float*)out->norm, 
		       (double2*)in->v, (float*)in->norm, 
		       dagger, kappa, mu, in->bytes, 
		       in->norm_bytes, twist, block);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    twistGamma5Cuda<4>((float4*)out->v, (float*)out->norm,
		       (float4*)in->v, (float*)in->norm, 
		       dagger, kappa, mu, in->bytes, 
		       in->norm_bytes, twist, block);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    twistGamma5Cuda<4>((short4*)out->v, (float*)out->norm,
		       (short4*)in->v, (float*)in->norm, 
		       dagger, kappa, mu, in->bytes, 
		       in->norm_bytes, twist, block);
  }
  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Twisted mass dslash has not been built");
#endif // GPU_TWISTED_MASS_DIRAC

}

// Twisted mass wrappers
template <int N, typename spinorFloat, typename gaugeFloat>
void twistedMassDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat gauge0, 
			   const gaugeFloat gauge1, const QudaReconstructType reconstruct, 
			   const spinorFloat *in, const float* inNorm, const int dagger, 
			   const spinorFloat *x, const float* xNorm, const double &kappa, 
			   const double &mu, const double &k, const int volume, 
			   const size_t bytes, const size_t norm_bytes, cudaStream_t &stream, 
			   const int shared_bytes, const dim3 blockDim)
{
  dim3 gridDim( (dslashParam.threads+blockDim.x-1) / blockDim.x, 1, 1);

  double a=0.0, b=0.0;
  setTwistParam(a, b, kappa, mu, dagger, QUDA_TWIST_GAMMA5_INVERSE);

  if (x==0) { // not xpay
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	twistedMassDslash18Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      } else {
	twistedMassDslash18DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	twistedMassDslash12Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      } else {
	twistedMassDslash12DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      }
    } else {
      if (!dagger) {
	twistedMassDslash8Kernel <<<gridDim, blockDim, shared_bytes, stream>>> 	
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      } else {
	twistedMassDslash8DaggerKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b);
      }
    }
  } else { // doing xpay
    b *= k;
    if (reconstruct == QUDA_RECONSTRUCT_NO) {
      if (!dagger) {
	twistedMassDslash18XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      } else {
	twistedMassDslash18DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	twistedMassDslash12XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      } else {
	twistedMassDslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      }
    } else {
      if (!dagger) {
	twistedMassDslash8XpayKernel <<<gridDim, blockDim, shared_bytes, stream>>> 	
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      } else {
	twistedMassDslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes, stream>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, dslashParam, a, b, x, xNorm);
      }
    }
  }
  
  unbindSpinorTex<N>(in, inNorm, x, xNorm);
}

template <int spinorN, typename spinorFloat, typename gaugeFloat>
void twistedMassDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat *gauge0, const gaugeFloat *gauge1, 
			   const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
			   const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
			   const double &kappa, const double &mu, const double &a, const int volume, 
			   const size_t bytes, const size_t norm_bytes, const dim3 block, const dim3 blockFace) {
  
  int shared_bytes = block.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);

  dslashParam.parity = parity;

#ifndef MULTI_GPU
  dslashParam.tOffset = 0;
  dslashParam.tMul = 1;
  dslashParam.threads = volume;

  twistedMassDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
				 dagger, x, xNorm, kappa, mu, a, volume, bytes, norm_bytes, 
				 streams[0], shared_bytes, block);
#else

  // Gather from source spinor
  face->exchangeFacesStart(*inSpinor, parity, dagger, streams);
  
#ifdef OVERLAP_COMMS // do body
  dslashParam.tOffset = 1;
  dslashParam.tMul = 1;
  dslashParam.threads = volume - 2*Vspatial;
  twistedMassDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
				 dagger, x, xNorm, kappa, mu, a, volume, bytes, norm_bytes, 
				 streams[Nstream-1], shared_bytes, block);    
#endif // OVERLAP_COMMS

  // Finish gather and start comms
  face->exchangeFacesComms();

  // Wait for comms to finish, and scatter into the end zone
  face->exchangeFacesWait(*inSpinor, dagger);

  dslashParam.tOffset = 0;
#ifdef OVERLAP_COMMS // do faces
  dslashParam.tMul = volume/Vspatial - 1; // hacky way to get Nt
  dslashParam.threads = 2*Vspatial;
#else // do all
  dslashParam.tMul = 1;
  dslashParam.threads = volume;
#endif // OVERLAP_COMMS
  shared_bytes = blockFace.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<spinorN>(bytes, norm_bytes, in, inNorm, x, xNorm);
  twistedMassDslashCuda<spinorN>(out, outNorm, gauge0, gauge1, reconstruct, in, inNorm, 
				 dagger, x, xNorm, kappa, mu, a, volume, bytes, norm_bytes, 
				 streams[Nstream-2], shared_bytes, blockFace);    

#endif // MULTI_GPU

  // texture unbinding is not asynchronous so we don't need to cudaThreadSynchonize()
  unbindSpinorTex<spinorN>(in, inNorm, x, xNorm); 
}

void twistedMassDslashCuda(cudaColorSpinorField *out, const FullGauge gauge, 
			   const cudaColorSpinorField *in, const int parity, const int dagger, 
			   const cudaColorSpinorField *x, const double &kappa, const double &mu, 
			   const double &a, const dim3 &block, const dim3 &blockFace) {

  inSpinor = (cudaColorSpinorField*)in; // EVIL

#ifdef GPU_TWISTED_MASS_DIRAC
  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (in->precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  void *xv = x ? x->v : 0;
  void *xn = x ? x->norm : 0;

  if (in->precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    twistedMassDslashCuda<2>((double2*)out->v, (float*)out->norm, (double2*)gauge0, (double2*)gauge1, 
			     gauge.reconstruct, (double2*)in->v, (float*)in->norm, 
			     parity, dagger, (double2*)xv, (float*)xn, kappa, mu, a, 
			     in->volume, in->bytes, in->norm_bytes, block, blockFace);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (in->precision == QUDA_SINGLE_PRECISION) {
    twistedMassDslashCuda<4>((float4*)out->v, (float*)out->norm, (float4*)gauge0, (float4*)gauge1, 
			     gauge.reconstruct, (float4*)in->v, (float*)in->norm, 
			     parity, dagger, (float4*)xv, (float*)xn, kappa, mu, a, 
			     in->volume, in->bytes, in->norm_bytes, block, blockFace);
  } else if (in->precision == QUDA_HALF_PRECISION) {
    twistedMassDslashCuda<4>((short4*)out->v, (float*)out->norm, (short4*)gauge0, (short4*)gauge1, 
			     gauge.reconstruct, (short4*)in->v, (float*)in->norm, parity, dagger, 
			     (short4*)xv, (float*)xn, kappa, mu, a, 
			     in->volume, in->bytes, in->norm_bytes, block, blockFace);

  }

  unbindGaugeTex(gauge);

  if (!dslashTuning) checkCudaError();
#else
  errorQuda("Twisted mass dslash has not been built");
#endif

}

#if defined(GPU_FATLINK)||defined(GPU_GAUGE_FORCE)|| defined(GPU_FERMION_FORCE)
#include <force_common.h>
#include "force_kernel_common.cu"
#endif

#ifdef GPU_FATLINK
#include "misc_helpers.cu"
#include "llfat_quda.cu"
#endif

#ifdef GPU_GAUGE_FORCE
#include "gauge_force_quda.cu"
#endif

#ifdef GPU_FERMION_FORCE
#include "fermion_force_quda.cu"
#endif
