#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include <test_util.h>
#include <blas_reference.h>
#include <staggered_dslash_reference.h>
#include <quda.h>
#include <string.h>
#include "misc.h"
#include "gauge_quda.h"

#ifdef MULTI_GPU
#include <face_quda.h>
#endif

#define MAX(a,b) ((a)>(b)?(a):(b))
#define mySpinorSiteSize 6

void *fatlink[4];
void *longlink[4];  

#ifdef MULTI_GPU
void* ghost_fatlink[4], *ghost_longlink[4];
#endif

int device = 0;

extern FullGauge cudaFatLinkPrecise;
extern FullGauge cudaFatLinkSloppy;
extern FullGauge cudaLongLinkPrecise;
extern FullGauge cudaLongLinkSloppy;

QudaReconstructType link_recon = QUDA_RECONSTRUCT_12;
QudaPrecision prec = QUDA_SINGLE_PRECISION;
QudaPrecision cpu_prec = QUDA_DOUBLE_PRECISION;

QudaReconstructType link_recon_sloppy = QUDA_RECONSTRUCT_INVALID;
QudaPrecision  prec_sloppy = QUDA_INVALID_PRECISION;
cpuColorSpinorField* in;
cpuColorSpinorField* out;
cpuColorSpinorField* ref;
cpuColorSpinorField* tmp;

static double tol = 1e-8;

static int testtype = 0;
static int sdim = 24;
static int tdim = 24;


static void end();

extern int Z[4];
extern int V;
extern int Vh;
static int Vs_x, Vs_y, Vs_z, Vs_t;
extern int Vsh_x, Vsh_y, Vsh_z, Vsh_t;
static int Vsh[4];

template<typename Float>
void constructSpinorField(Float *res) {
  for(int i = 0; i < Vh; i++) {
    for (int s = 0; s < 1; s++) {
      for (int m = 0; m < 3; m++) {
	res[i*(1*3*2) + s*(3*2) + m*(2) + 0] = rand() / (Float)RAND_MAX;
	res[i*(1*3*2) + s*(3*2) + m*(2) + 1] = rand() / (Float)RAND_MAX;
      }
    }
  }
}

void
setDimConstants(int *X)
{
  V = 1;
  for (int d=0; d< 4; d++) {
    V *= X[d];
    Z[d] = X[d];
  }
  Vh = V/2;

  Vs_x = X[1]*X[2]*X[3];
  Vs_y = X[0]*X[2]*X[3];
  Vs_z = X[0]*X[1]*X[3];
  Vs_t = X[0]*X[1]*X[2];


  Vsh_x = Vs_x/2;
  Vsh_y = Vs_y/2;
  Vsh_z = Vs_z/2;
  Vsh_t = Vs_t/2;

  Vsh[0] = Vsh_x;
  Vsh[1] = Vsh_y;
  Vsh[2] = Vsh_z;
  Vsh[3] = Vsh_t;
}

static void
set_params(QudaGaugeParam* gaugeParam, QudaInvertParam* inv_param,
	   int X1, int  X2, int X3, int X4,
	   QudaPrecision cpu_prec, QudaPrecision prec, QudaPrecision prec_sloppy,
	   QudaReconstructType link_recon, QudaReconstructType link_recon_sloppy,
	   double mass, double tol, int maxiter, double reliable_delta,
	   double tadpole_coeff
	   )
{
  gaugeParam->X[0] = X1;
  gaugeParam->X[1] = X2;
  gaugeParam->X[2] = X3;
  gaugeParam->X[3] = X4;

  gaugeParam->cpu_prec = cpu_prec;    
  gaugeParam->cuda_prec = prec;
  gaugeParam->reconstruct = link_recon;  
  gaugeParam->cuda_prec_sloppy = prec_sloppy;
  gaugeParam->reconstruct_sloppy = link_recon_sloppy;
  gaugeParam->gauge_fix = QUDA_GAUGE_FIXED_NO;
  gaugeParam->tadpole_coeff = tadpole_coeff;
  gaugeParam->t_boundary = QUDA_ANTI_PERIODIC_T;
  gaugeParam->gauge_order = QUDA_QDP_GAUGE_ORDER;
  gaugeParam->ga_pad = X1*X2*X3/2;

  inv_param->verbosity = QUDA_VERBOSE;
  inv_param->inv_type = QUDA_CG_INVERTER;    
  inv_param->mass = mass;
  inv_param->tol = tol;
  inv_param->maxiter = 500;
  inv_param->reliable_delta = 1e-3;

  inv_param->solution_type = QUDA_MATDAG_MAT_SOLUTION;
  inv_param->solve_type = QUDA_NORMEQ_PC_SOLVE;
  inv_param->matpc_type = QUDA_MATPC_EVEN_EVEN;
  inv_param->dagger = QUDA_DAG_NO;
  inv_param->mass_normalization = QUDA_MASS_NORMALIZATION;

  inv_param->cpu_prec = cpu_prec;
  inv_param->cuda_prec = prec; 
  inv_param->cuda_prec_sloppy = prec_sloppy;
  inv_param->preserve_source = QUDA_PRESERVE_SOURCE_YES;
  inv_param->dirac_order = QUDA_DIRAC_ORDER;
  inv_param->dslash_type = QUDA_ASQTAD_DSLASH;
  inv_param->dirac_tune = QUDA_TUNE_NO;
  inv_param->preserve_dirac = QUDA_PRESERVE_DIRAC_NO;
  inv_param->sp_pad = X1*X2*X3/2;
  inv_param->use_init_guess = QUDA_USE_INIT_GUESS_YES;
  inv_param->ghostDim[0] = true;
  inv_param->ghostDim[1] = true;
  inv_param->ghostDim[2] = true;
  inv_param->ghostDim[3] = true;

}

int
invert_test(void)
{
  QudaGaugeParam gaugeParam = newQudaGaugeParam();
  QudaInvertParam inv_param = newQudaInvertParam();

  double mass = 0.95;

  set_params(&gaugeParam, &inv_param,
	     sdim, sdim, sdim, tdim,
	     cpu_prec, prec, prec_sloppy,
	     link_recon, link_recon_sloppy, mass, tol, 500, 1e-3,
	     0.8);
  
  // this must be before the FaceBuffer is created (this is because it allocates pinned memory - FIXME)
  initQuda(device);

  setDims(gaugeParam.X);
  setDimConstants(gaugeParam.X);

  size_t gSize = (gaugeParam.cpu_prec == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
  for (int dir = 0; dir < 4; dir++) {
    fatlink[dir] = malloc(V*gaugeSiteSize*gSize);
    longlink[dir] = malloc(V*gaugeSiteSize*gSize);
  }
  
  construct_fat_long_gauge_field(fatlink, longlink, 1, gaugeParam.cpu_prec, &gaugeParam);
    
  for (int dir = 0; dir < 4; dir++) {
    for(int i = 0;i < V*gaugeSiteSize;i++){
      if (gaugeParam.cpu_prec == QUDA_DOUBLE_PRECISION){
	((double*)fatlink[dir])[i] = 0.5 *rand()/RAND_MAX;
      }else{
	((float*)fatlink[dir])[i] = 0.5* rand()/RAND_MAX;
      }
    }
  }

  
#ifdef MULTI_GPU

  //exchange_init_dims(gaugeParam.X);
  int ghost_link_len[4] = {
    Vs_x*gaugeSiteSize*gSize,
    Vs_y*gaugeSiteSize*gSize,
    Vs_z*gaugeSiteSize*gSize,
    Vs_t*gaugeSiteSize*gSize
  };

  for(int i=0;i < 4;i++){
    ghost_fatlink[i] = malloc(ghost_link_len[i]);
    ghost_longlink[i] = malloc(3*ghost_link_len[i]);
    if (ghost_fatlink[i] == NULL || ghost_longlink[i] == NULL){
      printf("ERROR: malloc failed for ghost fatlink or ghost longlink\n");
      exit(1);
    }
  }

  //exchange_cpu_links4dir(fatlink, ghost_fatlink, longlink, ghost_longlink, gaugeParam.cpu_prec);

  void *fat_send[4], *long_send[4];
  for(int i=0;i < 4;i++){
    fat_send[i] = malloc(ghost_link_len[i]);
    long_send[i] = malloc(3*ghost_link_len[i]);
  }

  set_dim(Z);
  pack_ghost(fatlink, fat_send, 1, gaugeParam.cpu_prec);
  pack_ghost(longlink, long_send, 3, gaugeParam.cpu_prec);

  int dummyFace = 1;
  FaceBuffer faceBuf (Z, 4, 18, dummyFace, gaugeParam.cpu_prec);
  faceBuf.exchangeCpuLink((void**)ghost_fatlink, (void**)fat_send, 1);
  faceBuf.exchangeCpuLink((void**)ghost_longlink, (void**)long_send, 3);

  for (int i=0; i<4; i++) {
    free(fat_send[i]);
    free(long_send[i]);
  }

#endif

 
  ColorSpinorParam csParam;
  csParam.fieldLocation = QUDA_CPU_FIELD_LOCATION;
  csParam.nColor=3;
  csParam.nSpin=1;
  csParam.nDim=4;
  for(int d = 0; d < 4; d++) {
    csParam.x[d] = gaugeParam.X[d];
  }
  csParam.x[0] /= 2;
  
  csParam.precision = inv_param.cpu_prec;
  csParam.pad = 0;
  csParam.siteSubset = QUDA_PARITY_SITE_SUBSET;
  csParam.siteOrder = QUDA_EVEN_ODD_SITE_ORDER;
  csParam.fieldOrder  = QUDA_SPACE_SPIN_COLOR_FIELD_ORDER;
  csParam.gammaBasis = QUDA_DEGRAND_ROSSI_GAMMA_BASIS;
  csParam.create = QUDA_ZERO_FIELD_CREATE;  
  in = new cpuColorSpinorField(csParam);  
  out = new cpuColorSpinorField(csParam);  
  ref = new cpuColorSpinorField(csParam);  
  tmp = new cpuColorSpinorField(csParam);  
  
  if (inv_param.cpu_prec == QUDA_SINGLE_PRECISION){
    constructSpinorField((float*)in->v);    
  }else{
    constructSpinorField((double*)in->v);
  }


  
  
#ifdef MULTI_GPU

  if(testtype == 6){
    record_gauge(fatlink, ghost_fatlink[3], Vsh_t,
		 longlink, ghost_longlink[3], 3*Vsh_t,
		 link_recon, link_recon_sloppy,
		 &gaugeParam);
   }else{
    gaugeParam.type = QUDA_ASQTAD_FAT_LINKS;
    gaugeParam.ga_pad = MAX(sdim*sdim*sdim/2, sdim*sdim*tdim/2);
    gaugeParam.reconstruct= gaugeParam.reconstruct_sloppy = QUDA_RECONSTRUCT_NO;
    loadGaugeQuda(fatlink, &gaugeParam);
    
    gaugeParam.type = QUDA_ASQTAD_LONG_LINKS;
    gaugeParam.ga_pad = 3*MAX(sdim*sdim*sdim/2, sdim*sdim*tdim/2);
    gaugeParam.reconstruct= link_recon;
    gaugeParam.reconstruct_sloppy = link_recon_sloppy;
    loadGaugeQuda(longlink, &gaugeParam);
  }

#else
  gaugeParam.type = QUDA_ASQTAD_FAT_LINKS;
  gaugeParam.reconstruct = gaugeParam.reconstruct_sloppy = QUDA_RECONSTRUCT_NO;
  loadGaugeQuda(fatlink, &gaugeParam);
  
  gaugeParam.type = QUDA_ASQTAD_LONG_LINKS;
  gaugeParam.reconstruct = link_recon;
  gaugeParam.reconstruct_sloppy = link_recon_sloppy;
  loadGaugeQuda(longlink, &gaugeParam);
#endif
  
  double time0 = -((double)clock()); // Start the timer
  
  unsigned long volume = Vh;
  unsigned long nflops=2*1187; //from MILC's CG routine
  double nrm2=0;
  double src2=0;
  int ret = 0;


  switch(testtype){
  case 0: //even
    volume = Vh;
    inv_param.solution_type = QUDA_MATPCDAG_MATPC_SOLUTION;
    inv_param.matpc_type = QUDA_MATPC_EVEN_EVEN;
    
    invertQuda(out->v, in->v, &inv_param);
    
    time0 += clock(); 
    time0 /= CLOCKS_PER_SEC;

#ifdef MULTI_GPU    
    matdagmat_mg4dir(ref, fatlink, ghost_fatlink, longlink, ghost_longlink, 
		     out, mass, 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp, QUDA_EVEN_PARITY);
#else
    matdagmat(ref->v, fatlink, longlink, out->v, mass, 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp->v, QUDA_EVEN_PARITY);
#endif
    
    mxpy(in->v, ref->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    nrm2 = norm_2(ref->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    src2 = norm_2(in->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    break;

  case 1: //odd
	
    volume = Vh;    
    inv_param.solution_type = QUDA_MATPCDAG_MATPC_SOLUTION;
    inv_param.matpc_type = QUDA_MATPC_ODD_ODD;
    invertQuda(out->v, in->v, &inv_param);	
    time0 += clock(); // stop the timer
    time0 /= CLOCKS_PER_SEC;
    
#ifdef MULTI_GPU
    matdagmat_mg4dir(ref, fatlink, ghost_fatlink, longlink, ghost_longlink, 
		     out, mass, 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp, QUDA_ODD_PARITY);
#else
    matdagmat(ref->v, fatlink, longlink, out->v, mass, 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp->v, QUDA_ODD_PARITY);	
#endif
    mxpy(in->v, ref->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    nrm2 = norm_2(ref->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    src2 = norm_2(in->v, Vh*mySpinorSiteSize, inv_param.cpu_prec);
	
    break;
    
  case 2: //full spinor

    errorQuda("full spinor not supported\n");
    break;
    
  case 3: //multi mass CG, even
  case 4:
  case 5:
  case 6:

#define NUM_OFFSETS 4
        
    nflops = 2*(1205 + 15* NUM_OFFSETS); //from MILC's multimass CG routine
    double masses[NUM_OFFSETS] ={5.05, 1.23, 2.64, 2.33};
    double offsets[NUM_OFFSETS];	
    int num_offsets =NUM_OFFSETS;
    void* outArray[NUM_OFFSETS];
    int len;
    
    cpuColorSpinorField* spinorOutArray[NUM_OFFSETS];
    spinorOutArray[0] = out;    
    for(int i=1;i < num_offsets; i++){
      spinorOutArray[i] = new cpuColorSpinorField(csParam);       
    }
    
    for(int i=0;i < num_offsets; i++){
      outArray[i] = spinorOutArray[i]->v;
    }

    for (int i=0; i< num_offsets;i++){
      offsets[i] = 4*masses[i]*masses[i];
    }
    
    len=Vh;
    volume = Vh;      
    inv_param.solution_type = QUDA_MATPCDAG_MATPC_SOLUTION;

    if (testtype == 3){
      inv_param.matpc_type = QUDA_MATPC_EVEN_EVEN;      
    } else if (testtype == 4||testtype == 6){
      inv_param.matpc_type = QUDA_MATPC_ODD_ODD;      
    }else { //testtype ==5
      errorQuda("test 5 not supported\n");
    }
    
    double residue_sq;
    if (testtype == 6){
      //invertMultiShiftQudaMixed(spinorOutArray, in->v, &inv_param, offsets, num_offsets, &residue_sq);
    }else{      
      invertMultiShiftQuda(outArray, in->v, &inv_param, offsets, num_offsets, &residue_sq);	
    }
    cudaThreadSynchronize();
    printfQuda("Final residue squred =%g\n", residue_sq);
    time0 += clock(); // stop the timer
    time0 /= CLOCKS_PER_SEC;
    
    printfQuda("done: total time = %g secs, %i iter / %g secs = %g gflops, \n", 
	       time0, inv_param.iter, inv_param.secs,
	       inv_param.gflops/inv_param.secs);
    
    
    printfQuda("checking the solution\n");
    QudaParity parity;
    if (inv_param.solve_type == QUDA_NORMEQ_SOLVE){
      //parity = QUDA_EVENODD_PARITY;
      errorQuda("full parity not supported\n");
    }else if (inv_param.matpc_type == QUDA_MATPC_EVEN_EVEN){
      parity = QUDA_EVEN_PARITY;
    }else if (inv_param.matpc_type == QUDA_MATPC_ODD_ODD){
      parity = QUDA_ODD_PARITY;
    }else{
      errorQuda("ERROR: invalid spinor parity \n");
      exit(1);
    }
    
    for(int i=0;i < num_offsets;i++){
      printfQuda("%dth solution: mass=%f, ", i, masses[i]);
#ifdef MULTI_GPU
      matdagmat_mg4dir(ref, fatlink, ghost_fatlink, longlink, ghost_longlink, 
		       spinorOutArray[i], masses[i], 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp, parity);
#else
      matdagmat(ref->v, fatlink, longlink, outArray[i], masses[i], 0, inv_param.cpu_prec, gaugeParam.cpu_prec, tmp->v, parity);
#endif
      mxpy(in->v, ref->v, len*mySpinorSiteSize, inv_param.cpu_prec);
      double nrm2 = norm_2(ref->v, len*mySpinorSiteSize, inv_param.cpu_prec);
      double src2 = norm_2(in->v, len*mySpinorSiteSize, inv_param.cpu_prec);
      
      printfQuda("relative residual, requested = %g, actual = %g\n", inv_param.tol, sqrt(nrm2/src2));

      //emperical, if the cpu residue is more than 2 order the target accuracy, the it fails to converge
      if (sqrt(nrm2/src2) > 100*inv_param.tol){
	ret |=1;
	errorQuda("Converge failed!\n");
      }
    }
    
    for(int i=1; i < num_offsets;i++){
      delete spinorOutArray[i];
    }

    
  }//switch
    

  if (testtype <=2){

    printfQuda("Relative residual, requested = %g, actual = %g\n", inv_param.tol, sqrt(nrm2/src2));
	
    printfQuda("done: total time = %g secs, %i iter / %g secs = %g gflops, \n", 
	       time0, inv_param.iter, inv_param.secs,
	       inv_param.gflops/inv_param.secs);
    
    //emperical, if the cpu residue is more than 2 order the target accuracy, the it fails to converge
    if (sqrt(nrm2/src2) > 100*inv_param.tol){
      ret = 1;
      errorQuda("Convergence failed!\n");
    }
  }

  end();
  return ret;
}



static void
end(void) 
{
  for(int i=0;i < 4;i++){
    free(fatlink[i]);
    free(longlink[i]);
  }
#ifdef MULTI_GPU
  for(int i=0;i < 4;i++){
    free(ghost_fatlink[i]);
    free(ghost_longlink[i]);
  }
#endif  

  delete in;
  delete out;
  delete ref;
  delete tmp;
  
  endQuda();
}


void
display_test_info()
{
  printfQuda("running the following test:\n");
    
  printfQuda("prec    sloppy_prec    link_recon  sloppy_link_recon test_type  S_dimension T_dimension\n");
  printfQuda("%s   %s             %s            %s            %s         %d          %d \n",
	 get_prec_str(prec),get_prec_str(prec_sloppy),
	 get_recon_str(link_recon), 
	 get_recon_str(link_recon_sloppy), get_test_type(testtype), sdim, tdim);     
  return ;
  
}

void
usage(char** argv )
{
  printfQuda("Usage: %s <args>\n", argv[0]);
  printfQuda("--prec         <double/single/half>     Spinor/gauge precision\n"); 
  printfQuda("--prec_sloppy  <double/single/half>     Spinor/gauge sloppy precision\n"); 
  printfQuda("--recon        <8/12>                   Long link reconstruction type\n"); 
  printfQuda("--test         <0/1/2/3/4/5>            Testing type(0=even, 1=odd, 2=full, 3=multimass even,\n" 
	 "                                                     4=multimass odd, 5=multimass full)\n"); 
  printfQuda("--tdim                                  T dimension\n");
  printfQuda("--sdim                                  S dimension\n");
  printfQuda("--help                                  Print out this message\n"); 
  exit(1);
  return ;
}


int main(int argc, char** argv)
{

  int xsize=1;
  int ysize=1;
  int zsize=1;
  int tsize=1;

  int i;
  for (i =1;i < argc; i++){
	
    if( strcmp(argv[i], "--help")== 0){
      usage(argv);
    }
	
    if( strcmp(argv[i], "--prec") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      prec = get_prec(argv[i+1]);
      i++;
      continue;	    
    }
    
    if( strcmp(argv[i], "--prec_sloppy") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      prec_sloppy =  get_prec(argv[i+1]);
      i++;
      continue;	    
    }
    
    
    if( strcmp(argv[i], "--recon") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      link_recon =  get_recon(argv[i+1]);
      i++;
      continue;	    
    }
    if( strcmp(argv[i], "--tol") == 0){
      float tmpf;
      if (i+1 >= argc){
        usage(argv);
      }
      sscanf(argv[i+1], "%f", &tmpf);
      if (tol <= 0){
        printf("ERROR: invalid tol(%f)\n", tmpf);
        usage(argv);
      }
      tol = tmpf;
      i++;
      continue;
    }


	
    if( strcmp(argv[i], "--recon_sloppy") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      link_recon_sloppy =  get_recon(argv[i+1]);
      i++;
      continue;	    
    }
	
    if( strcmp(argv[i], "--test") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      testtype = atoi(argv[i+1]);
      i++;
      continue;	    
    }

    if( strcmp(argv[i], "--cprec") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      cpu_prec= get_prec(argv[i+1]);
      i++;
      continue;
    }

    if( strcmp(argv[i], "--tdim") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      tdim= atoi(argv[i+1]);
      if (tdim < 0 || tdim > 128){
	printf("ERROR: invalid T dimention (%d)\n", tdim);
	usage(argv);
      }
      i++;
      continue;
    }		
    if( strcmp(argv[i], "--sdim") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      sdim= atoi(argv[i+1]);
      if (sdim < 0 || sdim > 128){
	printf("ERROR: invalid S dimention (%d)\n", sdim);
	usage(argv);
      }
      i++;
      continue;
    }
    if( strcmp(argv[i], "--device") == 0){
          if (i+1 >= argc){
              usage(argv);
          }
          device =  atoi(argv[i+1]);
          if (device < 0){
	    printf("Error: invalid device number(%d)\n", device);
              exit(1);
          }
          i++;
          continue;
    }

    if( strcmp(argv[i], "--xgridsize") == 0){
      if (i+1 >= argc){ 
        usage(argv);
      }     
      xsize =  atoi(argv[i+1]);
      if (xsize <= 0 ){
        errorQuda("Error: invalid X grid size");
      }
      i++;
      continue;     
    }

    if( strcmp(argv[i], "--ygridsize") == 0){
      if (i+1 >= argc){
        usage(argv);
      }     
      ysize =  atoi(argv[i+1]);
      if (ysize <= 0 ){
        errorQuda("Error: invalid Y grid size");
      }
      i++;
      continue;     
    }

    if( strcmp(argv[i], "--zgridsize") == 0){
      if (i+1 >= argc){
        usage(argv);
      }     
      zsize =  atoi(argv[i+1]);
      if (zsize <= 0 ){
        errorQuda("Error: invalid Z grid size");
      }
      i++;
      continue;
    }

    if( strcmp(argv[i], "--tgridsize") == 0){
      if (i+1 >= argc){
        usage(argv);
      }     
      tsize =  atoi(argv[i+1]);
      if (tsize <= 0 ){
        errorQuda("Error: invalid T grid size");
      }
      i++;
      continue;
    }

    printf("ERROR: Invalid option:%s\n", argv[i]);
    usage(argv);
  }


  if (prec_sloppy == QUDA_INVALID_PRECISION){
    prec_sloppy = prec;
  }
  if (link_recon_sloppy == QUDA_RECONSTRUCT_INVALID){
    link_recon_sloppy = link_recon;
  }
  
  display_test_info();

  int X[] = {xsize, ysize, zsize, tsize};
  initCommsQuda(argc, argv, X, 4);
  
  int ret = invert_test();

  endCommsQuda();

  return ret;
}
