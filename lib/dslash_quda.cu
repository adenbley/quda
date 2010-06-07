#include <stdlib.h>
#include <stdio.h>

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

#define BLOCK_DIM 64

#include <dslash_textures.h>
#include <dslash_constants.h>

// kludge to avoid '#include nested too deeply' error
#define DD_DAG 0
#include <dslash_def.h>
#undef DD_DAG
#define DD_DAG 1
#include <dslash_def.h>
#undef DD_DAG

#include <clover_def.h> // kernels for applying the clover term alone

#include <dslash_staggered_def.h> // kernels for staggered kernels

#include <blas_quda.h>
#include <sys/time.h>
#include "mpicomm.h"
#include "exchange_face.h"

extern void *fwd_nbr_spinor, *back_nbr_spinor;
extern void* f_norm, *b_norm;
cudaStream_t stream[2];

int dslashCudaSharedBytes(QudaPrecision precision) {
  return BLOCK_DIM*SHARED_FLOATS_PER_THREAD*precision;
}

template <int spinorN, typename spinorFloat, typename gaugeFloat>
void dslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat *gauge0, const gaugeFloat *gauge1, 
		const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
		const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
		const double &a, const int volume, const int length) {

  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<spinorN>(length, in, inNorm, x, xNorm);

  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	dslash12Kernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity);
      } else {
	dslash12DaggerKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity);
      }
    } else {
      if (!dagger) {
	dslash8Kernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity);
      } else {
	dslash8DaggerKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity);
      }
    }
  } else { // doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	dslash12XpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity, x, xNorm, a);
      } else {
	dslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity, x, xNorm, a);
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	dslash8XpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity, x, xNorm, a);
      } else {
	dslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, in, inNorm, parity, x, xNorm, a);
      }
    }
  }
  
}

// Wilson wrappers
void dslashCuda(void *out, void *outNorm, const FullGauge gauge, const void *in, const void *inNorm, 
		const int parity, const int dagger, const void *x, const void *xNorm, 
		const double k, const int volume, const int length, const QudaPrecision precision) {

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  if (precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    dslashCuda<2>((double2*)out, (float*)outNorm, (double2*)gauge0, (double2*)gauge1, 
		  gauge.reconstruct, (double2*)in, (float*)inNorm, parity, dagger, 
		  (double2*)x, (float*)xNorm, k, volume, length);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (precision == QUDA_SINGLE_PRECISION) {
    dslashCuda<4>((float4*)out, (float*)outNorm, (float4*)gauge0, (float4*)gauge1,
		  gauge.reconstruct, (float4*)in, (float*)inNorm, parity, dagger, 
		  (float4*)x, (float*)xNorm, k, volume, length);
  } else if (precision == QUDA_HALF_PRECISION) {
    dslashCuda<4>((short4*)out, (float*)outNorm, (short4*)gauge0, (short4*)gauge1,
		  gauge.reconstruct, (short4*)in, (float*)inNorm, parity, dagger, 
		  (short4*)x, (float*)xNorm, k, volume, length);
  }
  checkCudaError();

}


template <int N, typename spinorFloat, typename cloverFloat>
void cloverCuda(spinorFloat *out, float *outNorm, const cloverFloat *clover,
		const float *cloverNorm, const spinorFloat *in, const float *inNorm, 
		const int parity, const int volume, const int length)
{
  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<N>(length, in, inNorm);
  cloverKernel<<<gridDim, blockDim, shared_bytes>>> 
    (out, outNorm, clover, cloverNorm, in, inNorm, parity);
}

void cloverCuda(void *out, void *outNorm, const FullGauge gauge, const FullClover clover, 
		const void *in, const void *inNorm, const int parity, const int volume,
		const int length, const QudaPrecision precision) {

  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(clover, parity, &cloverP, &cloverNormP);

  if (precision != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  if (precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    cloverCuda<2>((double2*)out, (float*)outNorm, (double2*)cloverP, 
		  (float*)cloverNormP, (double2*)in, 
		  (float*)inNorm, parity, volume, length);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (precision == QUDA_SINGLE_PRECISION) {
    cloverCuda<4>((float4*)out, (float*)outNorm, (float4*)cloverP, 
		  (float*)cloverNormP, (float4*)in, 
		  (float*)inNorm, parity, volume, length);
  } else if (precision == QUDA_HALF_PRECISION) {
    cloverCuda<4>((short4*)out, (float*)outNorm, (short4*)cloverP, 
		  (float*)cloverNormP, (short4*)in,
		  (float*)inNorm, parity, volume, length);
  }
  checkCudaError();

}

// Clover wrappers
template <int N, typename spinorFloat, typename cloverFloat, typename gaugeFloat>
void cloverDslashCuda(spinorFloat *out, float *outNorm, const gaugeFloat gauge0, 
		      const gaugeFloat gauge1, const QudaReconstructType reconstruct, 
		      const cloverFloat *clover, const float *cloverNorm, const spinorFloat *in, 
		      const float* inNorm, const int parity, const int dagger, const spinorFloat *x, 
		      const float* xNorm, const double &a, const int volume, const int length)
{
  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex<N>(length, in, inNorm, x, xNorm);

  if (x==0) { // not xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	cloverDslash12Kernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity);
      } else {
	cloverDslash12DaggerKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity);
      }
    } else {
      if (!dagger) {
	cloverDslash8Kernel <<<gridDim, blockDim, shared_bytes>>> 	
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity);
      } else {
	cloverDslash8DaggerKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity);
      }
    }
  } else { // doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	cloverDslash12XpayKernel <<<gridDim, blockDim, shared_bytes>>> 
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity, x, xNorm, a);
      } else {
	cloverDslash12DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity, x, xNorm, a);
      }
    } else {
      if (!dagger) {
	cloverDslash8XpayKernel <<<gridDim, blockDim, shared_bytes>>> 	
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity, x, xNorm, a);
      } else {
	cloverDslash8DaggerXpayKernel <<<gridDim, blockDim, shared_bytes>>>
	  (out, outNorm, gauge0, gauge1, clover, cloverNorm, in, inNorm, parity, x, xNorm, a);
      }
    }
  }

}

void cloverDslashCuda(void *out, void *outNorm, const FullGauge gauge, const FullClover cloverInv,
		      const void *in, const void *inNorm, const int parity, const int dagger, 
		      const void *x, const void *xNorm, const double a, const int volume, 
		      const int length, const QudaPrecision precision) {

  void *cloverP, *cloverNormP;
  QudaPrecision clover_prec = bindCloverTex(cloverInv, parity, &cloverP, &cloverNormP);

  void *gauge0, *gauge1;
  bindGaugeTex(gauge, parity, &gauge0, &gauge1);

  if (precision != gauge.precision)
    errorQuda("Mixing gauge and spinor precision not supported");

  if (precision != clover_prec)
    errorQuda("Mixing clover and spinor precision not supported");

  if (precision == QUDA_DOUBLE_PRECISION) {
#if (__CUDA_ARCH__ >= 130)
    cloverDslashCuda<2>((double2*)out, (float*)outNorm, (double2*)gauge0, (double2*)gauge1, 
			gauge.reconstruct, (double2*)cloverP, (float*)cloverNormP, (double2*)in, 
			(float*)inNorm, parity, dagger, (double2*)x, (float*)xNorm, a, volume, length);
#else
    errorQuda("Double precision not supported on this GPU");
#endif
  } else if (precision == QUDA_SINGLE_PRECISION) {
    cloverDslashCuda<4>((float4*)out, (float*)outNorm, (float4*)gauge0, (float4*)gauge1, 
			gauge.reconstruct, (float4*)cloverP, (float*)cloverNormP, (float4*)in, 
			(float*)inNorm, parity, dagger, (float4*)x, (float*)xNorm, a, volume, length);
  } else if (precision == QUDA_HALF_PRECISION) {
    cloverDslashCuda<4>((short4*)out, (float*)outNorm, (short4*)gauge0, (short4*)gauge1, 
			gauge.reconstruct, (short4*)cloverP, (float*)cloverNormP, (short4*)in,
			(float*)inNorm, parity, dagger, (short4*)x, (float*)xNorm, a, volume, length);
  }

  checkCudaError();

}

#if 1
template <int spinorN, typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
  void staggeredDslashCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
			   const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
			   const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
			   const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
			   const double &a, const int volume, const int length, const int ghost_length,
			   cudaColorSpinorField* inSpinor) {
    

  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex_mg<spinorN>(length, ghost_length, in, inNorm, x, xNorm);

  cudaEvent_t start1, stop1;
  cudaEvent_t start2, stop2;
  cudaEventCreate(&start1);
  cudaEventCreate(&stop1);
  cudaEventCreate(&start2);
  cudaEventCreate(&stop2);
  
  struct timeval t0, t1;
  struct timeval t01, t02, t03;


  //cudaThreadSynchronize(); //sync whatever kernel that is not synced 

  gettimeofday(&t0, NULL);  	
  cudaEventRecord(start1, stream[0]);

  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12InternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											 longGauge0, longGauge1, in, inNorm, parity); CUERR;
      } else {
	staggeredDslash12DaggerInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
												longGauge0, longGauge1, in, inNorm, parity); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8){
	  
      if (!dagger) {
	staggeredDslash8InternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											 longGauge0, longGauge1, in, inNorm, parity); 
      } else {
	staggeredDslash8DaggerInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											      longGauge0, longGauge1, in, inNorm, parity); 
      }
    }else{
      errorQuda("Invalid reconstruct value(%d) in function %s\n", reconstruct, __FUNCTION__);
    }
  } else { // doing xpay
    
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12AxpyInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      } else {
	staggeredDslash12DaggerAxpyInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
												   longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	staggeredDslash8AxpyInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											    longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a);
      } else {
	staggeredDslash8DaggerAxpyInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
												  longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); 
      }
    }else{
      errorQuda("Invalid reconstruct value in function %s\n", __FUNCTION__);	  
    }    
  }


  exchange_gpu_spinor_start(inSpinor, fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, &stream[1]);


  cudaEventRecord(stop1, stream[0]);  
  gettimeofday(&t01, NULL);
  exchange_gpu_spinor_wait(inSpinor, fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, &stream[1]);
  gettimeofday(&t02, NULL);    
  cudaEventRecord(start2, stream[0]);

  cudaStreamSynchronize(stream[0]);
  gettimeofday(&t03, NULL);    

  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12BoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
											 longGauge0, longGauge1, in, inNorm, parity); CUERR;
      } else {
	staggeredDslash12DaggerBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1,
												longGauge0, longGauge1, in, inNorm, parity); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8){
      
      if (!dagger) {
	staggeredDslash8BoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											 longGauge0, longGauge1, in, inNorm, parity); CUERR;
      } else {
	staggeredDslash8DaggerBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value(%d) in function %s\n", reconstruct, __FUNCTION__);
    }
  } else { // doing xpay
    
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12AxpyBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      } else {
	staggeredDslash12DaggerAxpyBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
												   longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	staggeredDslash8AxpyBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											    longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      } else {
	staggeredDslash8DaggerAxpyBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
												  longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value in function %s\n", __FUNCTION__);	  
    }    
  }
  cudaEventRecord(stop2, stream[0]);  


  cudaThreadSynchronize(); CUERR;  
  gettimeofday(&t1, NULL);
  
  float internal_time, boundary_time;
  cudaEventElapsedTime(&internal_time, start1, stop1);
  cudaEventElapsedTime(&boundary_time, start2, stop2);
  /*  
  PRINTF("Dlsash time is %.2f(ms),exchange_walltime=%.2f,  internal_kernel=%.2f(ms), boundary_kernel=%.2f(ms)\n",
	 TDIFF(t1, t0)*1000 , TDIFF(t02,t01)*1000, internal_time, boundary_time);
  PRINTF("CPU_internaltime=%.2f, CPU_boundarytime=%.2f\n", TDIFF(t01,t0)*1000, TDIFF(t1, t02)*1000);
  PRINTF("t03-t02=%.2f, pure boundary kernel=%.2f, walltime for internal kernel (t03-t1)=%.2f\n", 
	 TDIFF(t03, t02)*1000, TDIFF(t1, t03)*1000, TDIFF(t03, t0)*1000);
  */
}
#else
template <int spinorN, typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
  void staggeredDslashCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
			   const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
			   const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
			   const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
			   const double &a, const int volume, const int length, const int ghost_length,
			   cudaColorSpinorField* inSpinor) {
    

  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);

  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex_mg<spinorN>(length, ghost_length, in, inNorm, x, xNorm);

  //cudaEvent_t start1, stop1;
  //cudaEvent_t start2, stop2;
  //cudaEventCreate(&start1);
  //cudaEventCreate(&stop1);
  //cudaEventCreate(&start2);
  //cudaEventCreate(&stop2);
  
  struct timeval t0, t1, t2;
  //struct timeval t01, t02;
  
  cudaThreadSynchronize();
  
  gettimeofday(&t0, NULL);
  exchange_gpu_spinor(inSpinor, fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, &stream[0]);
  cudaThreadSynchronize();
  gettimeofday(&t1, NULL);
  //cudaEventRecord(start1, stream[0]);
  
  if (x==0) { // not doing xpay
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12Kernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											 longGauge0, longGauge1, in, inNorm, parity); CUERR;
      } else {
	staggeredDslash12DaggerKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
												longGauge0, longGauge1, in, inNorm, parity); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8){
      
      if (!dagger) {
	staggeredDslash8Kernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											 longGauge0, longGauge1, in, inNorm, parity); CUERR;
      } else {
	staggeredDslash8DaggerKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											      longGauge0, longGauge1, in, inNorm, parity); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value(%d) in function %s\n", reconstruct, __FUNCTION__);
    }
  } else { // doing xpay
    
    if (reconstruct == QUDA_RECONSTRUCT_12) {
      if (!dagger) {
	staggeredDslash12AxpyKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      } else {
	staggeredDslash12DaggerAxpyKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
												   longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      }
    } else if (reconstruct == QUDA_RECONSTRUCT_8) {
      if (!dagger) {
	staggeredDslash8AxpyKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											    longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      } else {
	staggeredDslash8DaggerAxpyKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
												  longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
      }
    }else{
      errorQuda("Invalid reconstruct value in function %s\n", __FUNCTION__);	  
    }    
  }
  cudaThreadSynchronize();
  gettimeofday(&t2, NULL);  

  //cudaEventRecord(stop1, stream[0]);  
  //gettimeofday(&t01, NULL);
  //gettimeofday(&t02, NULL);    
  //  cudaEventRecord(start2, stream[0]);

 
  //cudaEventRecord(stop2, stream[0]);  
  //cudaThreadSynchronize(); CUERR;
  //gettimeofday(&t1, NULL);
  
  //float internal_time, boundary_time;
  //cudaEventElapsedTime(&internal_time, start1, stop1);
  //cudaEventElapsedTime(&boundary_time, start2, stop2);
  
  PRINTF("exchange_walltime=%.2f,  kernel time=%.2f(ms)\n",
	 TDIFF(t1, t0)*1000 , TDIFF(t2,t1)*1000);
  
  
}

#endif


//This function is a special case for 18(no) reconstruct long link
//The reason is to make the type match easier(e.g float2 instead of float4)

template <int spinorN, typename spinorFloat, typename fatGaugeFloat, typename longGaugeFloat>
  void staggeredDslashNoReconCuda(spinorFloat *out, float *outNorm, const fatGaugeFloat *fatGauge0, const fatGaugeFloat *fatGauge1, 
				  const longGaugeFloat* longGauge0, const longGaugeFloat* longGauge1, 
				  const QudaReconstructType reconstruct, const spinorFloat *in, const float *inNorm,
				  const int parity, const int dagger, const spinorFloat *x, const float *xNorm, 
				  const double &a, const int volume, const int length, const int ghost_length,
				  cudaColorSpinorField* inSpinor) 
{
  
  
  dim3 gridDim(volume/BLOCK_DIM, 1, 1);
  dim3 blockDim(BLOCK_DIM, 1, 1);
  
  int shared_bytes = blockDim.x*SHARED_FLOATS_PER_THREAD*bindSpinorTex_mg<spinorN>(length, ghost_length, in, inNorm, x, xNorm);
  
  if (x==0) { // not doing xpay
    if (!dagger) {
      staggeredDslash18InternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
										       longGauge0, longGauge1, in, inNorm, parity);CUERR;
    } else {
      staggeredDslash18DaggerInternalKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity);CUERR;
    }    
  } else { // doing xpay
    
    if (!dagger) {
      staggeredDslash18AxpyInternalKernel<<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
											  longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
    } else {
      staggeredDslash18DaggerAxpyInternalKernel<<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
												longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
    }          
  }
  exchange_gpu_spinor_start(inSpinor, fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, &stream[1]);   
  exchange_gpu_spinor_wait(inSpinor, fwd_nbr_spinor, back_nbr_spinor, f_norm, b_norm, &stream[1]); 
  
  if (x==0) { // not doing xpay
    if (!dagger) {
      staggeredDslash18BoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1, 
										       longGauge0, longGauge1, in, inNorm, parity);CUERR;
    } else {
      staggeredDslash18DaggerBoundaryKernel <<<gridDim, blockDim, shared_bytes, stream[0]>>> (out, outNorm, fatGauge0, fatGauge1, 
											      longGauge0, longGauge1, in, inNorm, parity);CUERR;
    }    
  } else { // doing xpay
    
    if (!dagger) {
      staggeredDslash18AxpyBoundaryKernel<<<gridDim, blockDim, shared_bytes, stream[0]>>>	(out, outNorm, fatGauge0, fatGauge1,
												 longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
    } else {
      staggeredDslash18DaggerAxpyBoundaryKernel<<<gridDim, blockDim, shared_bytes, stream[0]>>>(out, outNorm, fatGauge0, fatGauge1,
												longGauge0, longGauge1, in, inNorm, parity, x, xNorm, a); CUERR;
    }          
  }
  
  cudaThreadSynchronize();
}

#if (__CUDA_ARCH__ >= 200)
__global__ void dummykernel()
{
  
}

#endif
void staggeredDslashCuda(void *out, void *outNorm, const FullGauge fatGauge, const FullGauge longGauge, 
			 cudaColorSpinorField *in,
			 const int parity, const int dagger, const void *x, const void *xNorm, 
			 const double k, const int volume, const int length, const int ghost_length, const QudaPrecision precision) 
{

#if (__CUDA_ARCH__ >= 200)
  cudaFuncSetCacheConfig(dummykernel, cudaFuncCachePreferL1);
  dummykernel<<<1,1>>>();
#endif
  
  for(int i=0;i < 2 ;i ++){
    cudaStreamCreate(&stream[i]); CUERR;
  }

  void *fatGauge0, *fatGauge1;
  void* longGauge0, *longGauge1;
  bindFatGaugeTex(fatGauge, parity, &fatGauge0, &fatGauge1);
  bindLongGaugeTex(longGauge, parity, &longGauge0, &longGauge1);
    
  if (precision != fatGauge.precision || precision != longGauge.precision){
    errorQuda("Mixing gauge and spinor precision not supported");
  }
    


  if (precision == QUDA_DOUBLE_PRECISION) {
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((double2*)out, (float*)outNorm, (double2*)fatGauge0, (double2*)fatGauge1, 			       
				    (double2*)longGauge0, (double2*)longGauge1,
				    longGauge.reconstruct, (double2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
				    (double2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }else{
      staggeredDslashCuda<2>((double2*)out, (float*)outNorm, (double2*)fatGauge0, (double2*)fatGauge1, 			       
			     (double2*)longGauge0, (double2*)longGauge1,
			     longGauge.reconstruct, (double2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
			     (double2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }
    
  } else if (precision == QUDA_SINGLE_PRECISION) {
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((float2*)out, (float*)outNorm, (float2*)fatGauge0, (float2*)fatGauge1,
				    (float2*)longGauge0, (float2*)longGauge1,
				    longGauge.reconstruct, (float2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
				    (float2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }else{
      staggeredDslashCuda<2>((float2*)out, (float*)outNorm, (float2*)fatGauge0, (float2*)fatGauge1,
			     (float4*)longGauge0, (float4*)longGauge1,
			     longGauge.reconstruct, (float2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
			     (float2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }
  } else if (precision == QUDA_HALF_PRECISION) {	
    if (longGauge.reconstruct == QUDA_RECONSTRUCT_NO){
      staggeredDslashNoReconCuda<2>((short2*)out, (float*)outNorm, (short2*)fatGauge0, (short2*)fatGauge1,
				    (short2*)longGauge0, (short2*)longGauge1,
				    longGauge.reconstruct, (short2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
				    (short2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }else{
      staggeredDslashCuda<2>((short2*)out, (float*)outNorm, (short2*)fatGauge0, (short2*)fatGauge1,
			     (short4*)longGauge0, (short4*)longGauge1,
			     longGauge.reconstruct, (short2*)in->getV(), (float*)in->getNorm(), parity, dagger, 
			     (short2*)x, (float*)xNorm, k, volume, length, ghost_length, in);
    }
  }

  for (int i = 0; i < 2; i++) {
    cudaStreamDestroy(stream[i]);
  }
  
}


