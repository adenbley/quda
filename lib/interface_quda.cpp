#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <quda.h>
#include <quda_internal.h>
#include <blas_quda.h>
#include <gauge_quda.h>
#include <dirac_quda.h>
#include <dslash_quda.h>
#include <clover_quda.h>
#include <invert_quda.h>

#include <color_spinor_field.h>

#ifdef MULTI_GPU
#ifdef MPI_COMMS
#include <mpi.h>
#endif

#ifdef QMP_COMMS
#include <qmp.h>
#endif
#endif

#include "mpicomm.h"


#define spinorSiteSize 24 // real numbers per spinor

FullGauge cudaGaugePrecise;      // Wilson links
FullGauge cudaGaugeSloppy;

FullGauge cudaFatLinkPrecise;    // asqtad fat links
FullGauge cudaFatLinkSloppy;

FullGauge cudaLongLinkPrecise;   // asqtad long links
FullGauge cudaLongLinkSloppy;

FullClover cudaCloverPrecise;    // clover term
FullClover cudaCloverSloppy;

FullClover cudaCloverInvPrecise; // inverted clover term
FullClover cudaCloverInvSloppy;

// define newQudaGaugeParam() and newQudaInvertParam()
#define INIT_PARAM
#include "check_params.h"
#undef INIT_PARAM

// define (static) checkGaugeParam() and checkInvertParam()
#define CHECK_PARAM
#include "check_params.h"
#undef CHECK_PARAM

// define printQudaGaugeParam() and printQudaInvertParam()
#define PRINT_PARAM
#include "check_params.h"
#undef PRINT_PARAM

#ifdef QMP_COMMS
int rank_QMP;
int num_QMP;
extern bool qudaPt0;
extern bool qudaPtNm1;
#endif

#include "face_quda.h"

QudaVerbosity verbosity;
int verbose = 0;

Dirac *d = NULL;
Dirac *dSloppy = NULL;
bool diracCreation = false;
bool diracTune = false;

int getGpuCount()
{
  int count;
  cudaGetDeviceCount(&count);
  if (count <= 0){
    errorQuda("No devices supporting CUDA");
  }
  return count;
}

void initQuda(int dev)
{
  static int initialized = 0;
  if (initialized) {
    return;
  }
  initialized = 1;


  int deviceCount;
  cudaGetDeviceCount(&deviceCount);
  if (deviceCount == 0) {
    errorQuda("No devices supporting CUDA");
  }

  for(int i=0; i<deviceCount; i++) {
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, i);
    printfQuda("QUDA: Found device %d: %s\n", i, deviceProp.name);
  }

#ifdef QMP_COMMS
  int ndim;
  const int *dim;
  const int *coords;

  if ( QMP_is_initialized() != QMP_TRUE ) {
    errorQuda("QMP is not initialized");
  }
  num_QMP=QMP_get_number_of_nodes();
  rank_QMP=QMP_get_node_number();
  
  dev += rank_QMP % deviceCount;
  ndim = QMP_get_logical_number_of_dimensions();
  dim = QMP_get_logical_dimensions();
  if (ndim != 4) { 
    errorQuda("This code needs 4 logical dimensions");
  }
  if(  (dim[0] != 1) || (dim[1] != 1) || (dim[2] != 1) )  { 
    errorQuda("This code needs all spatial dimensions local for now");
  }
  coords = QMP_get_logical_coordinates();
  if( coords[3] == 0 ) qudaPt0=true;
  else qudaPt0=false;

  if( coords[3] == dim[3]-1 ) qudaPtNm1=true;
  else qudaPtNm1=false;

#elif defined(MPI_COMMS)

  comm_init();
  dev=comm_gpuid();

#else
  if (dev < 0) dev = deviceCount - 1;
#endif


  cudaDeviceProp deviceProp;
  cudaGetDeviceProperties(&deviceProp, dev);
  if (deviceProp.major < 1) {
    errorQuda("Device %d does not support CUDA", dev);
  }

  
  printfQuda("QUDA: Using device %d: %s\n", dev, deviceProp.name);

  cudaSetDevice(dev);

  cudaGaugePrecise.even = NULL;
  cudaGaugePrecise.odd = NULL;
  cudaGaugeSloppy.even = NULL;
  cudaGaugeSloppy.odd = NULL;

  cudaFatLinkPrecise.even = NULL;
  cudaFatLinkPrecise.odd = NULL;
  cudaFatLinkSloppy.even = NULL;
  cudaFatLinkSloppy.odd = NULL;

  cudaLongLinkPrecise.even = NULL;
  cudaLongLinkPrecise.odd = NULL;
  cudaLongLinkSloppy.even = NULL;
  cudaLongLinkSloppy.odd = NULL;

  cudaCloverPrecise.even.clover = NULL;
  cudaCloverPrecise.odd.clover = NULL;
  cudaCloverSloppy.even.clover = NULL;
  cudaCloverSloppy.odd.clover = NULL;

  cudaCloverInvPrecise.even.clover = NULL;
  cudaCloverInvPrecise.odd.clover = NULL;
  cudaCloverInvSloppy.even.clover = NULL;
  cudaCloverInvSloppy.odd.clover = NULL;

  initCache();
  initBlas();
}



void loadGaugeQuda(void *h_gauge, QudaGaugeParam *param)
{

  double anisotropy;
  FullGauge *precise, *sloppy;

  checkGaugeParam(param);

  param->packed_size = param->reconstruct;

  switch (param->type) {
  case QUDA_WILSON_LINKS:
    precise = &cudaGaugePrecise;
    sloppy = &cudaGaugeSloppy;
    break;
  case QUDA_ASQTAD_FAT_LINKS:
    precise = &cudaFatLinkPrecise;
    sloppy = &cudaFatLinkSloppy;
    break;
  case QUDA_ASQTAD_LONG_LINKS:
    precise = &cudaLongLinkPrecise;
    sloppy = &cudaLongLinkSloppy;
    break;
  default:
    errorQuda("Invalid gauge type");   
  }

  if (param->type == QUDA_WILSON_LINKS) {
    anisotropy = param->anisotropy;
  } else {
    anisotropy = 1.0;
  }

  if (param->type != QUDA_WILSON_LINKS &&
      param->gauge_fix == QUDA_GAUGE_FIXED_YES) {
    errorQuda("Temporal gauge fixing not supported for staggered");
  }

  if ((param->cuda_prec == QUDA_HALF_PRECISION && param->reconstruct == QUDA_RECONSTRUCT_NO) ||
      (param->cuda_prec_sloppy == QUDA_HALF_PRECISION && param->reconstruct_sloppy == QUDA_RECONSTRUCT_NO)) {
    warningQuda("Loading gauge field in half precision may give wrong results "
		"unless all elements have magnitude bounded by 1");
  }

  createGaugeField(precise, h_gauge, param->cuda_prec, param->cpu_prec, param->gauge_order, param->reconstruct, param->gauge_fix,
		   param->t_boundary, param->X, anisotropy, param->tadpole_coeff, param->ga_pad, param->type);
  param->gaugeGiB += 2.0 * precise->bytes / (1 << 30);

  if (param->cuda_prec_sloppy != param->cuda_prec ||
      param->reconstruct_sloppy != param->reconstruct) {
    createGaugeField(sloppy, h_gauge, param->cuda_prec_sloppy, param->cpu_prec, param->gauge_order,
		     param->reconstruct_sloppy, param->gauge_fix, param->t_boundary,
		     param->X, anisotropy, param->tadpole_coeff, param->ga_pad, param->type);
    param->gaugeGiB += 2.0 * sloppy->bytes / (1 << 30);
  } else {
    // This has copy semantics... 
    // It is equivalent to 
    // qudaGaugeSloppy = qudaGaugePrecise
    //   NB: even and odd in qudaGaugeSloppy point to even and odd in qudaGaugePrecise... 
    *sloppy = *precise;
  }

  endInvertQuda(); // need to delete any persistant dirac operators
}




void saveGaugeQuda(void *h_gauge, QudaGaugeParam *param)
{
  FullGauge *gauge = NULL;

  switch (param->type) {
  case QUDA_WILSON_LINKS:
    gauge = &cudaGaugePrecise;
    break;
  case QUDA_ASQTAD_FAT_LINKS:
    gauge = &cudaFatLinkPrecise;
    break;
  case QUDA_ASQTAD_LONG_LINKS:
    gauge = &cudaLongLinkPrecise;
    break;
  default:
    errorQuda("Invalid gauge type");   
  }

  restoreGaugeField(h_gauge, gauge, param->cpu_prec, param->gauge_order);
}


void loadCloverQuda(void *h_clover, void *h_clovinv, QudaInvertParam *inv_param)
{

  if (!h_clover && !h_clovinv) {
    errorQuda("loadCloverQuda() called with neither clover term nor inverse");
  }
  if (inv_param->clover_cpu_prec == QUDA_HALF_PRECISION) {
    errorQuda("Half precision not supported on CPU");
  }
  if (cudaGaugePrecise.even == NULL) {
    errorQuda("Gauge field must be loaded before clover");
  }
  if (inv_param->dslash_type != QUDA_CLOVER_WILSON_DSLASH) {
    errorQuda("Wrong dslash_type in loadCloverQuda()");
  }

  // determines whether operator is preconditioned when calling invertQuda()
  bool pc_solve = (inv_param->solve_type == QUDA_DIRECT_PC_SOLVE ||
		   inv_param->solve_type == QUDA_NORMEQ_PC_SOLVE);

  // determines whether operator is preconditioned when calling MatQuda() or MatDagMatQuda()
  bool pc_solution = (inv_param->solution_type == QUDA_MATPC_SOLUTION ||
		      inv_param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION);

  bool asymmetric = (inv_param->matpc_type == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC ||
		     inv_param->matpc_type == QUDA_MATPC_ODD_ODD_ASYMMETRIC);

  // We issue a warning only when it seems likely that the user is screwing up:

  // inverted clover term is required when applying preconditioned operator
  if (!h_clovinv && pc_solve && pc_solution) {
    warningQuda("Inverted clover term not loaded");
  }

  // uninverted clover term is required when applying unpreconditioned operator,
  // but note that dslashQuda() is always preconditioned
  if (!h_clover && !pc_solve && !pc_solution) {
    //warningQuda("Uninverted clover term not loaded");
  }

  // uninverted clover term is also required for "asymmetric" preconditioning
  if (!h_clover && pc_solve && pc_solution && asymmetric) {
    warningQuda("Uninverted clover term not loaded");
  }

  int X[4];
  for (int i=0; i<4; i++) {
    X[i] = cudaGaugePrecise.X[i];
  }
  X[0] /= 2; // X defines the full lattice now
  // FIXME: clover should take the full lattice dims not the CB dims

  inv_param->cloverGiB = 0;

  if (h_clover) {
    allocateCloverField(&cudaCloverPrecise, X, inv_param->cl_pad, inv_param->clover_cuda_prec);
    loadCloverField(cudaCloverPrecise, h_clover, inv_param->clover_cpu_prec, inv_param->clover_order);
    inv_param->cloverGiB += 2.0*cudaCloverPrecise.even.bytes / (1<<30);

    if (inv_param->clover_cuda_prec != inv_param->clover_cuda_prec_sloppy) {
      allocateCloverField(&cudaCloverSloppy, X, inv_param->cl_pad, inv_param->clover_cuda_prec_sloppy);
      loadCloverField(cudaCloverSloppy, h_clover, inv_param->clover_cpu_prec, inv_param->clover_order);
      inv_param->cloverGiB += 2.0*cudaCloverInvSloppy.even.bytes / (1<<30);
    } else {
      cudaCloverSloppy = cudaCloverPrecise;
    }
  }

  if (h_clovinv) {
    allocateCloverField(&cudaCloverInvPrecise, X, inv_param->cl_pad, inv_param->clover_cuda_prec);
    loadCloverField(cudaCloverInvPrecise, h_clovinv, inv_param->clover_cpu_prec, inv_param->clover_order);
    inv_param->cloverGiB += 2.0*cudaCloverInvPrecise.even.bytes / (1<<30);
    
    if (inv_param->clover_cuda_prec != inv_param->clover_cuda_prec_sloppy) {
      allocateCloverField(&cudaCloverInvSloppy, X, inv_param->cl_pad, inv_param->clover_cuda_prec_sloppy);
      loadCloverField(cudaCloverInvSloppy, h_clovinv, inv_param->clover_cpu_prec, inv_param->clover_order);
      inv_param->cloverGiB += 2.0*cudaCloverInvSloppy.even.bytes / (1<<30);
    } else {
      cudaCloverInvSloppy = cudaCloverInvPrecise;
    }

    // FIXME: hack to allow tuning of DiracClover with only cloverInv defined
    if (!h_clover) {
      cudaCloverPrecise = cudaCloverInvPrecise;
      cudaCloverSloppy = cudaCloverInvSloppy;
    }
  }

  endInvertQuda(); // need to delete any persistant dirac operators
}


#if 0
// discard clover term but keep the inverse
void discardCloverQuda(QudaInvertParam *inv_param)
{
  inv_param->cloverGiB -= 2.0*cudaCloverPrecise.even.bytes / (1<<30);
  freeCloverField(&cudaCloverPrecise);
  if (cudaCloverSloppy.even.clover) {
    inv_param->cloverGiB -= 2.0*cudaCloverSloppy.even.bytes / (1<<30);
    freeCloverField(&cudaCloverSloppy);
  }
}
#endif

void freeGaugeQuda(void) 
{
  if ( cudaGaugeSloppy.even == cudaGaugePrecise.even ) { 
	// Buffer shared between precise and sloppy.
	// Free the precise one, and set the sloppy pointers to NULL
	freeGaugeField(&cudaGaugePrecise);
	cudaGaugeSloppy.even = NULL;
	cudaGaugeSloppy.odd = NULL;
  }
  else { 
	freeGaugeField(&cudaGaugePrecise);
        freeGaugeField(&cudaGaugeSloppy);
  }

  if ( cudaFatLinkSloppy.even == cudaFatLinkPrecise.even ) {
        // Buffer shared between precise and sloppy.
        // Free the precise one, and set the sloppy pointers to NULL
        freeGaugeField(&cudaFatLinkPrecise);
        cudaFatLinkSloppy.even = NULL;
        cudaFatLinkSloppy.odd = NULL;
  }
  else {
        freeGaugeField(&cudaFatLinkPrecise);
        freeGaugeField(&cudaFatLinkSloppy);
  }

  if ( cudaLongLinkSloppy.even == cudaLongLinkPrecise.even ) {
        // Buffer shared between precise and sloppy.
        // Free the precise one, and set the sloppy pointers to NULL
        freeGaugeField(&cudaLongLinkPrecise);
        cudaLongLinkSloppy.even = NULL;
        cudaLongLinkSloppy.odd = NULL;
  }
  else {
        freeGaugeField(&cudaLongLinkPrecise);
        freeGaugeField(&cudaLongLinkSloppy);
  }

}

void freeCloverQuda(void)
{
    bool sameCloverEven = ( cudaCloverPrecise.even.clover == cudaCloverSloppy.even.clover );
    bool sameCloverOdd = ( cudaCloverPrecise.odd.clover == cudaCloverSloppy.odd.clover );
    bool sameCloverInvEven = ( cudaCloverInvPrecise.even.clover == cudaCloverInvSloppy.even.clover );
    bool sameCloverInvOdd = ( cudaCloverInvPrecise.odd.clover == cudaCloverInvSloppy.odd.clover ) ; 

    if ( sameCloverEven || sameCloverOdd ) { 
      freeCloverField(&cudaCloverPrecise);
      cudaCloverSloppy.even.clover = NULL;
      cudaCloverSloppy.even.cloverNorm = NULL;
      cudaCloverSloppy.odd.clover = NULL;
      cudaCloverSloppy.odd.cloverNorm = NULL;
    }
    else { 
      freeCloverField(&cudaCloverPrecise);
      freeCloverField(&cudaCloverSloppy);
    }
 
    if ( sameCloverInvEven || sameCloverInvOdd ) {
      freeCloverField(&cudaCloverInvPrecise ); 
      cudaCloverInvSloppy.even.clover = NULL;
      cudaCloverInvSloppy.odd.clover = NULL;
      cudaCloverInvSloppy.odd.cloverNorm = NULL;
      cudaCloverInvSloppy.even.cloverNorm = NULL;
    }
    else { 
	freeCloverField(&cudaCloverInvPrecise);
        freeCloverField(&cudaCloverInvSloppy);
    } 
}

void endQuda(void)
{
  endInvertQuda();

  cudaColorSpinorField::freeBuffer();
  freeGaugeQuda();
  freeCloverQuda();

  endBlas();
}


void setDiracParam(DiracParam &diracParam, QudaInvertParam *inv_param, const bool pc)
{
  double kappa = inv_param->kappa;
  if (inv_param->dirac_order == QUDA_CPS_WILSON_DIRAC_ORDER) {
    kappa *= cudaGaugePrecise.anisotropy;
  }

  switch (inv_param->dslash_type) {
  case QUDA_WILSON_DSLASH:
    diracParam.type = pc ? QUDA_WILSONPC_DIRAC : QUDA_WILSON_DIRAC;
    break;
  case QUDA_CLOVER_WILSON_DSLASH:
    diracParam.type = pc ? QUDA_CLOVERPC_DIRAC : QUDA_CLOVER_DIRAC;
    break;
  case QUDA_DOMAIN_WALL_DSLASH:
    diracParam.type = pc ? QUDA_DOMAIN_WALLPC_DIRAC : QUDA_DOMAIN_WALL_DIRAC;
    break;
  case QUDA_ASQTAD_DSLASH:
    diracParam.type = pc ? QUDA_ASQTADPC_DIRAC : QUDA_ASQTAD_DIRAC;
    break;
  case QUDA_TWISTED_MASS_DSLASH:
    diracParam.type = pc ? QUDA_TWISTED_MASSPC_DIRAC : QUDA_TWISTED_MASS_DIRAC;
    break;
  default:
    errorQuda("Unsupported dslash_type");
  }

  diracParam.matpcType = inv_param->matpc_type;
  diracParam.dagger = inv_param->dagger;
  diracParam.gauge = &cudaGaugePrecise;
  diracParam.fatGauge = &cudaFatLinkPrecise;
  diracParam.longGauge = &cudaLongLinkPrecise;    
  diracParam.clover = &cudaCloverPrecise;
  diracParam.cloverInv = &cudaCloverInvPrecise;
  diracParam.kappa = kappa;
  diracParam.mass = inv_param->mass;
  diracParam.m5 = inv_param->m5;
  diracParam.mu = inv_param->mu;
  diracParam.verbose = inv_param->verbosity;

}


void setDiracSloppyParam(DiracParam &diracParam, QudaInvertParam *inv_param, const bool pc)
{
  setDiracParam(diracParam, inv_param, pc);

  diracParam.gauge = &cudaGaugeSloppy;
  diracParam.fatGauge = &cudaFatLinkSloppy;
  diracParam.longGauge = &cudaLongLinkSloppy;    
  diracParam.clover = &cudaCloverSloppy;
  diracParam.cloverInv = &cudaCloverInvSloppy;
}


static void massRescale(QudaDslashType dslash_type, double &kappa, QudaSolutionType solution_type, 
			QudaMassNormalization mass_normalization, cudaColorSpinorField &b)
{    
  if (dslash_type == QUDA_ASQTAD_DSLASH) {
    if (mass_normalization != QUDA_MASS_NORMALIZATION) {
      errorQuda("Staggered code only supports QUDA_MASS_NORMALIZATION");
    }
    return;
  }

  // multiply the source to compensate for normalization of the Dirac operator, if necessary
  switch (solution_type) {
  case QUDA_MAT_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION ||
	mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(2.0*kappa, b);
    }
    break;
  case QUDA_MATDAG_MAT_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION ||
	mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(4.0*kappa*kappa, b);
    }
    break;
  case QUDA_MATPC_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION) {
	axCuda(4.0*kappa*kappa, b);
    } else if (mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
	axCuda(2.0*kappa, b);
    }
    break;
  case QUDA_MATPCDAG_MATPC_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION) {
	axCuda(16.0*pow(kappa,4), b);
    } else if (mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
	axCuda(4.0*kappa*kappa, b);
    }
    break;
  default:
    errorQuda("Solution type %d not supported", solution_type);
  }

  if (verbosity >= QUDA_DEBUG_VERBOSE) printfQuda("Mass rescale done\n");   
}

static void massRescaleCoeff(QudaDslashType dslash_type, double &kappa, QudaSolutionType solution_type, 
			     QudaMassNormalization mass_normalization, double &coeff)
{    
  if (dslash_type == QUDA_ASQTAD_DSLASH) {
    if (mass_normalization != QUDA_MASS_NORMALIZATION) {
      errorQuda("Staggered code only supports QUDA_MASS_NORMALIZATION");
    }
    return;
  }

  // multiply the source to compensate for normalization of the Dirac operator, if necessary
  switch (solution_type) {
  case QUDA_MAT_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION ||
	mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      coeff *= 2.0*kappa;
    }
    break;
  case QUDA_MATDAG_MAT_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION ||
	mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      coeff *= 4.0*kappa*kappa;
    }
    break;
  case QUDA_MATPC_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION) {
      coeff *= 4.0*kappa*kappa;
    } else if (mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      coeff *= 2.0*kappa;
    }
    break;
  case QUDA_MATPCDAG_MATPC_SOLUTION:
    if (mass_normalization == QUDA_MASS_NORMALIZATION) {
	coeff*=16.0*pow(kappa,4);
    } else if (mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
	coeff*=4.0*kappa*kappa;
    }
    break;
  default:
    errorQuda("Solution type %d not supported", solution_type);
  }

  if (verbosity >= QUDA_DEBUG_VERBOSE) printfQuda("Mass rescale done\n");   
}


void dslashQuda(void *h_out, void *h_in, QudaInvertParam *inv_param, QudaParity parity)
{
  ColorSpinorParam cpuParam(h_in, *inv_param, cudaGaugePrecise.X, 1);
  ColorSpinorParam cudaParam(cpuParam, *inv_param);

  cpuColorSpinorField hIn(cpuParam);

  cudaColorSpinorField in(hIn, cudaParam);

  cudaParam.create = QUDA_NULL_FIELD_CREATE;
  cudaColorSpinorField out(in, cudaParam);

  if (inv_param->dirac_order == QUDA_CPS_WILSON_DIRAC_ORDER) {
    if (parity == QUDA_EVEN_PARITY) {
      parity = QUDA_ODD_PARITY;
    } else {
      parity = QUDA_EVEN_PARITY;
    }
    axCuda(cudaGaugePrecise.anisotropy, in);
  }
  bool pc = true;

  DiracParam diracParam;
  setDiracParam(diracParam, inv_param, pc);

  Dirac *dirac = Dirac::create(diracParam); // create the Dirac operator
  dirac->Dslash(out, in, parity); // apply the operator
  delete dirac; // clean up

  cpuParam.v = h_out;
  cpuColorSpinorField hOut(cpuParam);
  out.saveCPUSpinorField(hOut); // since this is a reference, this won't work: hOut = out;
}


void MatQuda(void *h_out, void *h_in, QudaInvertParam *inv_param)
{
  bool pc = (inv_param->solution_type == QUDA_MATPC_SOLUTION ||
	     inv_param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION);

  ColorSpinorParam cpuParam(h_in, *inv_param, cudaGaugePrecise.X, pc);

  ColorSpinorParam cudaParam(cpuParam, *inv_param);

  cpuColorSpinorField hIn(cpuParam);
  cudaColorSpinorField in(hIn, cudaParam);
  cudaParam.create = QUDA_NULL_FIELD_CREATE;
  cudaColorSpinorField out(in, cudaParam);

  DiracParam diracParam;
  setDiracParam(diracParam, inv_param, pc);

  Dirac *dirac = Dirac::create(diracParam); // create the Dirac operator
  dirac->M(out, in); // apply the operator
  delete dirac; // clean up

  double kappa = inv_param->kappa;
  if (pc) {
    if (inv_param->mass_normalization == QUDA_MASS_NORMALIZATION) {
      axCuda(0.25/(kappa*kappa), out);
    } else if (inv_param->mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(0.5/kappa, out);
    }
  } else {
    if (inv_param->mass_normalization == QUDA_MASS_NORMALIZATION ||
	inv_param->mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(0.5/kappa, out);
    }
  }

  cpuParam.v = h_out;
  cpuColorSpinorField hOut(cpuParam);
  out.saveCPUSpinorField(hOut); // since this is a reference, this won't work: hOut = out;
}


void MatDagMatQuda(void *h_out, void *h_in, QudaInvertParam *inv_param)
{
  bool pc = (inv_param->solution_type == QUDA_MATPC_SOLUTION ||
	     inv_param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION);

  ColorSpinorParam cpuParam(h_in, *inv_param, cudaGaugePrecise.X, pc);
  ColorSpinorParam cudaParam(cpuParam, *inv_param);

  cpuColorSpinorField hIn(cpuParam);
  cudaColorSpinorField in(hIn, cudaParam);
  cudaParam.create = QUDA_NULL_FIELD_CREATE;
  cudaColorSpinorField out(in, cudaParam);

  //  double kappa = inv_param->kappa;
  //  if (inv_param->dirac_order == QUDA_CPS_WILSON_DIRAC_ORDER) kappa *= cudaGaugePrecise.anisotropy;

  DiracParam diracParam;
  setDiracParam(diracParam, inv_param, pc);

  Dirac *dirac = Dirac::create(diracParam); // create the Dirac operator
  dirac->MdagM(out, in); // apply the operator
  delete dirac; // clean up

  double kappa = inv_param->kappa;
  if (pc) {
    if (inv_param->mass_normalization == QUDA_MASS_NORMALIZATION) {
      axCuda(1.0/pow(2.0*kappa,4), out);
    } else if (inv_param->mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(0.25/(kappa*kappa), out);
    }
  } else {
    if (inv_param->mass_normalization == QUDA_MASS_NORMALIZATION ||
	inv_param->mass_normalization == QUDA_ASYMMETRIC_MASS_NORMALIZATION) {
      axCuda(0.25/(kappa*kappa), out);
    }
  }

  cpuParam.v = h_out;
  cpuColorSpinorField hOut(cpuParam);
  out.saveCPUSpinorField(hOut); // since this is a reference, this won't work: hOut = out;
}

void createDirac(DiracParam &diracParam, QudaInvertParam &param, bool pc_solve) {
  if (!diracCreation) {
    setDiracParam(diracParam, &param, pc_solve);
    d = Dirac::create(diracParam); // create the Dirac operator    
    setDiracSloppyParam(diracParam, &param, pc_solve);
    dSloppy = Dirac::create(diracParam);
    diracCreation = true;
  }
}

// tune the Dirac operators
void tuneDirac(QudaInvertParam &param, const cudaColorSpinorField &x) {
  if (param.dirac_tune == QUDA_TUNE_YES && !diracTune) {
    { // tune Dirac operator
      cudaColorSpinorField a = x;
      cudaColorSpinorField b = x;
      cudaColorSpinorField c = x;
      d->Tune(a, b, c);
    }

    { // tune slopppy Dirac operator
      ColorSpinorParam CSparam(x);
      CSparam.precision = param.cuda_prec_sloppy;
      CSparam.create = QUDA_NULL_FIELD_CREATE;
      cudaColorSpinorField a(x, CSparam);
      cudaColorSpinorField b(x, CSparam);
      cudaColorSpinorField c(x, CSparam);
      dSloppy->Tune(a, b, c);
    }
    diracTune = true;
  }
}

void invertQuda(void *hp_x, void *hp_b, QudaInvertParam *param)
{
  checkInvertParam(param);
  verbosity = param->verbosity;

  bool pc_solve = (param->solve_type == QUDA_DIRECT_PC_SOLVE ||
		   param->solve_type == QUDA_NORMEQ_PC_SOLVE);

  bool pc_solution = (param->solution_type == QUDA_MATPC_SOLUTION ||
		      param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION);

  param->spinorGiB = cudaGaugePrecise.volumeCB * spinorSiteSize;
  if (!pc_solve) param->spinorGiB *= 2;
  param->spinorGiB *= (param->cuda_prec == QUDA_DOUBLE_PRECISION ? sizeof(double) : sizeof(float));
  if (param->preserve_source == QUDA_PRESERVE_SOURCE_NO) {
    param->spinorGiB *= (param->inv_type == QUDA_CG_INVERTER ? 5 : 7)/(double)(1<<30);
  } else {
    param->spinorGiB *= (param->inv_type == QUDA_CG_INVERTER ? 8 : 9)/(double)(1<<30);
  }

  param->secs = 0;
  param->gflops = 0;
  param->iter = 0;

  // create the dirac operator
  DiracParam diracParam;
  createDirac(diracParam, *param, pc_solve);
  Dirac &dirac = *d;
  Dirac &diracSloppy = *dSloppy;

  cpuColorSpinorField *h_b = NULL;
  cpuColorSpinorField *h_x = NULL;
  cudaColorSpinorField *b = NULL;
  cudaColorSpinorField *x = NULL;
  cudaColorSpinorField *in = NULL;
  cudaColorSpinorField *out = NULL;

  int *X = param->dslash_type == QUDA_ASQTAD_DSLASH ? 
    cudaFatLinkPrecise.X : cudaGaugePrecise.X;

  // wrap CPU host side pointers
  ColorSpinorParam cpuParam(hp_b, *param, X, pc_solution);
  h_b = new cpuColorSpinorField(cpuParam);
  cpuParam.v = hp_x;
  h_x = new cpuColorSpinorField(cpuParam);
    
  // download source
  ColorSpinorParam cudaParam(cpuParam, *param);     
  cudaParam.create = QUDA_COPY_FIELD_CREATE;
  b = new cudaColorSpinorField(*h_b, cudaParam); 

  if (param->use_init_guess == QUDA_USE_INIT_GUESS_YES) { // download initial guess
    x = new cudaColorSpinorField(*h_x, cudaParam); // solution  
  } else { // zero initial guess
    cudaParam.create = QUDA_ZERO_FIELD_CREATE;
    x = new cudaColorSpinorField(cudaParam); // solution
  }
    
  if (param->verbosity >= QUDA_VERBOSE) {
    double nh_b = norm2(*h_b);
    double nb = norm2(*b);
    printfQuda("Source: CPU = %f, CUDA copy = %f\n", nh_b, nb);
  }

  tuneDirac(*param, pc_solution ? *x : x->Even());

  dirac.prepare(in, out, *x, *b, param->solution_type);
  if (param->verbosity >= QUDA_VERBOSE) {
    double nin = norm2(*in);
    printfQuda("Prepared source = %f\n", nin);   
  }

  massRescale(param->dslash_type, diracParam.kappa, param->solution_type, param->mass_normalization, *in);

  switch (param->inv_type) {
  case QUDA_CG_INVERTER:
    if (param->solution_type != QUDA_MATDAG_MAT_SOLUTION && param->solution_type != QUDA_MATPCDAG_MATPC_SOLUTION) {
      copyCuda(*out, *in);
      dirac.Mdag(*in, *out);
    }
    invertCgCuda(DiracMdagM(dirac), DiracMdagM(diracSloppy), *out, *in, param);
    break;
  case QUDA_BICGSTAB_INVERTER:
    if (param->solution_type == QUDA_MATDAG_MAT_SOLUTION || param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION) {
      invertBiCGstabCuda(DiracMdag(dirac), DiracMdag(diracSloppy), *out, *in, param);
      copyCuda(*in, *out);
    }
    invertBiCGstabCuda(DiracM(dirac), DiracM(diracSloppy), *out, *in, param);
    break;
  default:
    errorQuda("Inverter type %d not implemented", param->inv_type);
  }
  
  if (param->verbosity >= QUDA_VERBOSE){
   double nx = norm2(*x);
   printfQuda("Solution = %f\n",nx);
  }
  dirac.reconstruct(*x, *b, param->solution_type);
  
  x->saveCPUSpinorField(*h_x); // since this is a reference, this won't work: h_x = x;
  
  if (param->verbosity >= QUDA_VERBOSE){
    double nx = norm2(*x);
    double nh_x = norm2(*h_x);
    printfQuda("Reconstructed: CUDA solution = %f, CPU copy = %f\n", nx, nh_x);
  }
  
  if (!param->preserve_dirac) {
    delete d;
    delete dSloppy;
    diracCreation = false;
    diracTune = false;
  }  

  delete h_b;
  delete h_x;
  delete b;
  delete x;
  
  return;
}


/*! 
 *
 * Generic version of the multi-shift solver. Should work for
 * most fermions. Note, offset[0] is not folded into the mass parameter 
 */
void invertMultiShiftQuda(void **_hp_x, void *_hp_b, QudaInvertParam *param,
			  double* offsets, int num_offsets, double* residue_sq)
{
  checkInvertParam(param);
  verbosity = param->verbosity;

  // Are we doing a preconditioned solve */
  /* What does NormEq solve mean in the shifted case? 
   */
  if (param->solve_type != QUDA_NORMEQ_PC_SOLVE &&
      param->solve_type != QUDA_NORMEQ_SOLVE) { 
    errorQuda("Direct solve_type is not supported in invertMultiShiftQuda()\n");
  }

  bool pc_solve = (param->solve_type == QUDA_NORMEQ_PC_SOLVE);

  // In principle one can do a MATPC Solution for a hermitian M_pc
  // In practice most of the time I guess one will do a M^\dagger_pc M_pc solution.
  bool pc_solution = (param->solution_type == QUDA_MATPC_SOLUTION ||
		      param->solution_type == QUDA_MATPCDAG_MATPC_SOLUTION );

  // No of GiB in a checkerboard of a spinor
  param->spinorGiB = cudaGaugePrecise.volumeCB * spinorSiteSize;
  if( !pc_solve) param->spinorGiB *= 2; // Double volume for non PC solve
  
  // **** WARNING *** this may not match implementation... 
  if( param->inv_type == QUDA_CG_INVERTER ) { 
    // CG-M needs 5 vectors for the smallest shift + 2 for each additional shift
    param->spinorGiB *= (5 + 2*(num_offsets-1))/(double)(1<<30);
  }
  else {
    // BiCGStab-M needs 7 for the original shift + 2 for each additional shift + 1 auxiliary
    // (Jegerlehner hep-lat/9612014 eq (3.13)
    param->spinorGiB *= (7 + 2*(num_offsets-1))/(double)(1<<30);
  }

  // Timing and FLOP counters
  param->secs = 0;
  param->gflops = 0;
  param->iter = 0;
  
  // Find the smallest shift and its offset.
  double low_offset = offsets[0];
  int low_index = 0;
  for (int i=1;i < num_offsets;i++){
    if (offsets[i] < low_offset){
      low_offset = offsets[i];
      low_index = i;
    }
  }
  
  // Host pointers for x, take a copy of the input host pointers
  void** hp_x;
  hp_x = new void* [ num_offsets ];

  void* hp_b = _hp_b;
  for(int i=0;i < num_offsets;i++){
    hp_x[i] = _hp_x[i];
  }
  
  // Now shift things so that the vector with the smallest shift 
  // is in the first position of the array
  if (low_index != 0){
    void* tmp = hp_x[0];
    hp_x[0] = hp_x[low_index] ;
    hp_x[low_index] = tmp;
    
    double tmp1 = offsets[0];
    offsets[0]= offsets[low_index];
    offsets[low_index] =tmp1;
  }
    
  // Create the matrix.
  // The way this works is that createDirac will create 'd' and 'dSloppy'
  // which are global. We then grab these with references...
  //
  // Balint: Isn't there a  nice construction pattern we could use here? This is 
  // expedient but yucky.
  DiracParam diracParam; 
  if (param->dslash_type == QUDA_ASQTAD_DSLASH){
    param->mass = sqrt(offsets[0]/4);  
  }
  createDirac(diracParam, *param, pc_solve);
  Dirac &dirac = *d;
  Dirac &diracSloppy = *dSloppy;

  cpuColorSpinorField *h_b = NULL; // Host RHS
  cpuColorSpinorField **h_x = NULL;
  cudaColorSpinorField *b = NULL;   // Cuda RHS
  cudaColorSpinorField **x = NULL;  // Cuda Solutions

  // Grab the dimension array of the input gauge field.
  int *X = ( param->dslash_type == QUDA_ASQTAD_DSLASH ) ? 
    cudaFatLinkPrecise.X : cudaGaugePrecise.X;

  // Wrap CPU host side pointers
  // 
  // Balint: This creates a ColorSpinorParam struct, from the host data pointer, 
  // the definitions in param, the dimensions X, and whether the solution is on 
  // a checkerboard instruction or not. These can then be used as 'instructions' 
  // to create the actual colorSpinorField
  ColorSpinorParam cpuParam(hp_b, *param, X, pc_solution);
  h_b = new cpuColorSpinorField(cpuParam);

  h_x = new cpuColorSpinorField* [ num_offsets ]; // DYNAMIC ALLOCATION
  for(int i=0; i < num_offsets; i++) { 
    cpuParam.v = hp_x[i];
    h_x[i] = new cpuColorSpinorField(cpuParam);
  }

  // Now I need a colorSpinorParam for the device
  ColorSpinorParam cudaParam(cpuParam, *param);
  // This setting will download a host vector
  cudaParam.create = QUDA_COPY_FIELD_CREATE;
  b = new cudaColorSpinorField(*h_b, cudaParam); // Creates b and downloads h_b to it

  // Create the solution fields filled with zero
  x = new cudaColorSpinorField* [ num_offsets ];
  cudaParam.create = QUDA_ZERO_FIELD_CREATE;
  for(int i=0; i < num_offsets; i++) { 
    x[i] = new cudaColorSpinorField(cudaParam);
  }

  // Check source norms
  if( param->verbosity >= QUDA_VERBOSE ) {
    double nh_b = norm2(*h_b);
    double nb = norm2(*b);
    printfQuda("Source: CPU= %f, CUDA copy = %f\n", nh_b,nb);
  }

  // tune the Dirac Kernel
  tuneDirac(*param, pc_solution ? *(x[0]) : (x[0])->Even());
  
  
  massRescale(param->dslash_type, diracParam.kappa, param->solution_type, param->mass_normalization, *b);
  double *rescaled_shifts = new double [num_offsets];
  for(int i=0; i < num_offsets; i++){ 
    rescaled_shifts[i] = offsets[i];
    massRescaleCoeff(param->dslash_type, diracParam.kappa, param->solution_type, param->mass_normalization, rescaled_shifts[i]);
  }
  invertMultiShiftCgCuda(DiracMdagM(dirac), DiracMdagM(diracSloppy), x, *b, param, rescaled_shifts, num_offsets, residue_sq);
  
  
  delete [] rescaled_shifts;

  for(int i=0; i < num_offsets; i++) { 
    x[i]->saveCPUSpinorField(*h_x[i]);
  }

  for(int i=0; i < num_offsets; i++){ 
    delete h_x[i];
    delete x[i];
  }

  delete h_b;
  delete b;

  delete [] h_x;
  delete [] x;

  delete [] hp_x;

  if (!param->preserve_dirac) {
    delete d;
    delete dSloppy;
    diracCreation = false;
    diracTune = false;
  }  

  return;
}

void endInvertQuda() {
  if (diracCreation) {
    delete d;
    delete dSloppy;
    diracCreation = false;
    diracTune = false;
  }
}


/************************************** Mixed precision multishift CG solver ****************************/

static void* fatlink;
static void* ghost_fatlink;
static int fatlink_pad;
static void* longlink;
static void* ghost_longlink;
static int longlink_pad;
static QudaReconstructType longlink_recon;
static QudaReconstructType longlink_recon_sloppy;
static QudaGaugeParam* gauge_param;

void 
record_gauge(void *_fatlink, void* _ghost_fatlink, int _fatlink_pad, 
	     void* _longlink, void* _ghost_longlink, int _longlink_pad, 
	     QudaReconstructType _longlink_recon,QudaReconstructType _longlink_recon_sloppy,
	     QudaGaugeParam *_param)
{
  fatlink = _fatlink;
  ghost_fatlink = _ghost_fatlink;
  fatlink_pad = _fatlink_pad;
  
  longlink = _longlink;
  ghost_longlink = _ghost_longlink;
  longlink_pad = _longlink_pad;
  longlink_recon = _longlink_recon;
  longlink_recon_sloppy = _longlink_recon_sloppy;
  
  gauge_param = _param;

  return;

}


void 
do_create_precise_cuda_gauge(void)
{
  QudaPrecision prec = gauge_param->cuda_prec;
  QudaPrecision prec_sloppy = gauge_param->cuda_prec_sloppy;
  
  //create precise links
  gauge_param->cuda_prec = gauge_param->cuda_prec_sloppy = prec;
  gauge_param->type = QUDA_ASQTAD_FAT_LINKS;
  gauge_param->ga_pad = fatlink_pad;
  gauge_param->reconstruct = gauge_param->reconstruct_sloppy = QUDA_RECONSTRUCT_NO;
  loadGaugeQuda(fatlink, gauge_param);

  
  gauge_param->type = QUDA_ASQTAD_LONG_LINKS;
  gauge_param->ga_pad = longlink_pad;
  gauge_param->reconstruct = longlink_recon;
  gauge_param->reconstruct_sloppy = longlink_recon_sloppy;
  loadGaugeQuda(longlink, gauge_param);

  //set prec/prec_sloppy it back
  gauge_param->cuda_prec = prec;
  gauge_param->cuda_prec_sloppy =prec_sloppy;
  
  return;
}

void 
do_create_sloppy_cuda_gauge(void)
{
  QudaPrecision prec = gauge_param->cuda_prec;
  QudaPrecision prec_sloppy = gauge_param->cuda_prec_sloppy;  

  //create sloppy links
  gauge_param->cuda_prec = gauge_param->cuda_prec_sloppy = prec_sloppy; 
  gauge_param->type = QUDA_ASQTAD_FAT_LINKS;
  gauge_param->ga_pad = fatlink_pad;
  gauge_param->reconstruct = gauge_param->reconstruct_sloppy = QUDA_RECONSTRUCT_NO;
  loadGaugeQuda(fatlink, gauge_param);
  
  gauge_param->type = QUDA_ASQTAD_LONG_LINKS;
  gauge_param->ga_pad = longlink_pad;
  gauge_param->reconstruct = longlink_recon;
  gauge_param->reconstruct_sloppy = longlink_recon_sloppy;
  loadGaugeQuda(longlink, gauge_param);
  
  //set prec/prec_sloppy it back
  gauge_param->cuda_prec = prec;
  gauge_param->cuda_prec_sloppy =prec_sloppy;
  
  return;
}

void 
invertMultiShiftQudaMixed(void **_hp_x, void *_hp_b, QudaInvertParam *param,
			  double* offsets, int num_offsets, double* residue_sq)
{

  int i;
  QudaPrecision high_prec = param->cuda_prec;
  QudaPrecision low_prec = param->cuda_prec_sloppy;
  if (num_offsets <= 0){
    return;
  }

#ifdef MULTI_GPU   
  do_create_sloppy_cuda_gauge();  
#endif

  bool pc_solve = (param->solve_type == QUDA_NORMEQ_PC_SOLVE);

  //checkInvertParam(param);
  param->secs = 0;
  param->gflops = 0;
  param->iter = 0;
  
  double low_offset = offsets[0];
  int low_index = 0;
  for (int i=1;i < num_offsets;i++){
    if (offsets[i] < low_offset){
      low_offset = offsets[i];
      low_index = i;
    }
  }
  
  void* hp_x[num_offsets];
  void* hp_b = _hp_b;
  for(int i=0;i < num_offsets;i++){
    hp_x[i] = _hp_x[i];
  }
  
  if (low_index != 0){
    void* tmp = hp_x[0];
    hp_x[0] = hp_x[low_index] ;
    hp_x[low_index] = tmp;
    
    double tmp1 = offsets[0];
    offsets[0]= offsets[low_index];
    offsets[low_index] =tmp1;
  }
  
  
  ColorSpinorParam csParam;
  csParam.precision = param->cpu_prec;
  csParam.fieldLocation = QUDA_CPU_FIELD_LOCATION;
  csParam.gammaBasis = QUDA_DEGRAND_ROSSI_GAMMA_BASIS; 
  csParam.fieldOrder = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
  csParam.nColor=3;
  csParam.nSpin=1;
  csParam.nDim=4;
  csParam.siteOrder = QUDA_EVEN_ODD_SITE_ORDER;
  
  if (param->solve_type == QUDA_NORMEQ_SOLVE) {
    csParam.siteSubset = QUDA_FULL_SITE_SUBSET;
    csParam.x[0] = cudaFatLinkSloppy.X[0];
  } else if (param->solve_type == QUDA_NORMEQ_PC_SOLVE) {
    csParam.siteSubset = QUDA_PARITY_SITE_SUBSET;
    csParam.x[0] = cudaFatLinkSloppy.X[0]/2; // gauge->X defines the full lattice volume 
  } else {
    errorQuda("Direct solve_type not supported for staggered");
  }

  csParam.x[1] = cudaFatLinkSloppy.X[1];
  csParam.x[2] = cudaFatLinkSloppy.X[2];
  csParam.x[3] = cudaFatLinkSloppy.X[3];
  csParam.create = QUDA_REFERENCE_FIELD_CREATE;
  csParam.v = hp_b;  
  cpuColorSpinorField h_b(csParam);
  
  cpuColorSpinorField* h_x[num_offsets];
  
  for(i=0;i < num_offsets; i++){
    csParam.v = hp_x[i];
    h_x[i] = new cpuColorSpinorField(csParam);
  }
  
  csParam.fieldLocation = QUDA_CUDA_FIELD_LOCATION;
  csParam.fieldOrder = QUDA_FLOAT2_FIELD_ORDER;
  csParam.gammaBasis = QUDA_DEGRAND_ROSSI_GAMMA_BASIS;

  csParam.pad = param->sp_pad;
  csParam.precision = low_prec;
  param->cuda_prec = low_prec;
  csParam.create = QUDA_ZERO_FIELD_CREATE;
  
  cudaColorSpinorField *b = new cudaColorSpinorField(csParam);
  
  //set the mass in the invert_param
  param->mass = sqrt(offsets[0]/4);

  DiracParam diracParam;
  createDirac(diracParam, *param, pc_solve);
  Dirac &dirac = *d;
  Dirac &diracSloppy = *dSloppy;
  
  *b= h_b; //send data from cpu to GPU
  
  csParam.create = QUDA_ZERO_FIELD_CREATE;
  cudaColorSpinorField* x_sloppy[num_offsets]; // solution  
  for(i=0;i < num_offsets;i++){
    x_sloppy[i] = new cudaColorSpinorField(csParam);
  }
  invertMultiShiftCgCuda(DiracMdagM(diracSloppy), DiracMdagM(diracSloppy), x_sloppy, *b, param, offsets, num_offsets, residue_sq);   

  delete b;

  int total_iters = 0;
  double total_secs = 0;
  double total_gflops = 0;

  total_iters += param->iter;
  total_secs  += param->secs;
  total_gflops += param->gflops;

  param->cuda_prec = high_prec;
  csParam.precision = high_prec;
  csParam.create = QUDA_ZERO_FIELD_CREATE;
  b = new cudaColorSpinorField(csParam);
  *b = h_b;

#ifdef MULTI_GPU   
  do_create_precise_cuda_gauge();
#endif

  cudaColorSpinorField* x;
  x = new cudaColorSpinorField(csParam);
  for(i=0;i < num_offsets; i++){
    *x  = *x_sloppy[i];
    delete x_sloppy[i];
    double mass = sqrt(offsets[i]/4);
    dirac.setMass(mass);
    diracSloppy.setMass(mass);
    invertCgCuda(DiracMdagM(dirac), DiracMdagM(diracSloppy), *x, *b, param);
      
    total_iters += param->iter;
    total_secs  += param->secs;
    total_gflops += param->gflops;

    x->saveCPUSpinorField(*h_x[i]);
    	
  }
  
  param->iter = total_iters;
  param->secs = total_secs;
  param->gflops = total_gflops;

  if (!param->preserve_dirac) {
    delete d;
    delete dSloppy;
    diracCreation = false;
    diracTune = false;
  } 
  
  for(i=0;i < num_offsets; i++) delete h_x[i];
  delete x;
  delete b;
  
  return;
}

void initCommsQuda(int argc, char **argv, const int *X, const int nDim) {

  if (nDim != 4) errorQuda("Comms dimensions %d != 4", nDim);

#ifdef MULTI_GPU

#ifdef QMP_COMMS
  QMP_thread_level_t tl;
  QMP_init_msg_passing(&argc, &argv, QMP_THREAD_SINGLE, &tl);

  if (X[0] != 1 && X[1] != 1 && X[2] != 1) 
    errorQuda("QMP only supports T-dimension parallelization");
  if (X[3] != QMP_get_number_of_nodes())
    errorQuda("Number of processes %d must match requested T-dimension QMP grid size %d", 
	      QMP_get_number_of_nodes(), X[3]);

  QMP_declare_logical_topology(X, nDim);
#elif defined(MPI_COMMS)
  MPI_Init (&argc, &argv);  

  int volume = 1;
  for (int d=0; d<nDim; d++) volume *= X[d];
  int size = -1;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (volume != size)
    errorQuda("Number of processes %d must match requested MPI volume %d",
	      size, volume);

  comm_set_gridsize(X[0], X[1], X[2], X[3]);  
  comm_init();
#endif

#endif
}

void endCommsQuda() {
#ifdef MULTI_GPU

#ifdef QMP_COMMS
  QMP_finalize_msg_passing();
#elif defined MPI_COMMS
  comm_cleanup();
#endif 

#endif
}

