#ifndef _QUDA_H
#define _QUDA_H

#include <enum_quda.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct QudaGaugeParam_s {

    int X[4];

    double anisotropy;

    QudaGaugeFieldOrder gauge_order;

    QudaTboundary t_boundary;

    QudaPrecision cpu_prec;

    QudaPrecision cuda_prec;
    QudaReconstructType reconstruct;

    QudaPrecision cuda_prec_sloppy;
    QudaReconstructType reconstruct_sloppy;

    QudaGaugeFixed gauge_fix;

    int blockDim; // number of threads in a block
    int blockDim_sloppy;

    int ga_pad;

    int packed_size;
    double gaugeGiB;
    QudaGaugeType type;

  } QudaGaugeParam;

  typedef struct QudaInvertParam_s {

    QudaGaugeParam* gaugeParam;
    QudaDslashType dslash_type;
    QudaInverterType inv_type;
    QudaParity in_parity;

    double mass;
    double kappa;  
    double tol;
    int maxiter;
    double reliable_delta; // reliable update tolerance

    QudaSolutionType solution_type; // type of system to solve
    QudaSolutionType solver_type; // how to solve it
    QudaMatPCType matpc_type;
    QudaMassNormalization mass_normalization;

    QudaPreserveSource preserve_source;

    QudaPrecision cpu_prec;
    QudaPrecision cuda_prec;
    QudaPrecision cuda_prec_sloppy;

    QudaDiracFieldOrder dirac_order;

    QudaPrecision clover_cpu_prec;
    QudaPrecision clover_cuda_prec;
    QudaPrecision clover_cuda_prec_sloppy;

    QudaCloverFieldOrder clover_order;

    QudaVerbosity verbosity;

    int sp_pad;
    int cl_pad;

    int iter;
    double spinorGiB;
    double cloverGiB;
    double gflops;
    double secs;

  } QudaInvertParam;

  // Interface functions, found in interface_quda.cpp

  void initQuda(int dev);
  void loadGaugeQuda(void *h_gauge, QudaGaugeParam *param);
  void loadGaugeQuda_general(void *h_gauge, QudaGaugeParam *param, void*, void*);
  void loadGaugeQuda_general_mg(void *h_gauge, void* ghost_gauge, QudaGaugeParam *param, void* _cudaLinkPrecise, void* _cudaLinkSloppy, int num_faces);
  void saveGaugeQuda(void *h_gauge, QudaGaugeParam *param);
  void loadCloverQuda(void *h_clover, void *h_clovinv, QudaInvertParam *inv_param);

  void invertQuda(void *h_x, void *h_b, QudaInvertParam *param);
  void invertQudaSt(void *h_x, void *h_b, QudaInvertParam *param);
  void invertQudaStMultiMass(void **_hp_x, void *_hp_b, QudaInvertParam *param,
			     double* offsets, int num_offsets, double* residue_sq);
    
  void dslashQuda(void *h_out, void *h_in, QudaInvertParam *inv_param, int parity, QudaDagType dagger);
  void MatPCQuda(void *h_out, void *h_in, QudaInvertParam *inv_param, QudaDagType dagger);
  void MatPCDagMatPCQuda(void *h_out, void *h_in, QudaInvertParam *inv_param);
  void MatQuda(void *h_out, void *h_in, QudaInvertParam *inv_param, QudaDagType dagger);

  void endQuda(void);

  QudaGaugeParam newQudaGaugeParam(void);
  QudaInvertParam newQudaInvertParam(void);

  void printQudaGaugeParam(QudaGaugeParam *param);
  void printQudaInvertParam(QudaInvertParam *param);

#define CUERR  do{ cudaError_t cuda_err;                                \
    if ((cuda_err = cudaGetLastError()) != cudaSuccess) {		\
      fprintf(stderr, "ERROR: CUDA error: %s, line %d, function %s, file %s\n", \
	      cudaGetErrorString(cuda_err),  __LINE__, __FUNCTION__, __FILE__); \
      exit(cuda_err);}}while(0)
  
  


#ifdef __cplusplus
}
#endif

#endif // _QUDA_H
