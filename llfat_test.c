#include <stdio.h>
#include <stdlib.h>

#include <quda.h>
#include <dslash_reference.h>
#include <util_quda.h>
#include <spinor_quda.h>
#include <gauge_quda.h>
#include <string.h>
#include "misc.h"
#include "llfat_reference.h"
#include "llfat_quda.h"
#include "dslash_quda.h"
#include <sys/time.h>

FullGauge cudaSiteLink;
FullGauge cudaFatLink;
FullStaple cudaStaple;
FullStaple cudaStaple1;
QudaGaugeParam gaugeParam;
void *fatLink, *siteLink, *refLink;
int verify_results = 0;

extern void initDslashCuda(FullGauge gauge);

#define DIM 24

int ODD_BIT = 1;
int tdim = 16;
int sdim = 16;

QudaReconstructType link_recon = QUDA_RECONSTRUCT_12;
QudaPrecision  link_prec = QUDA_SINGLE_PRECISION;

typedef struct {
    double real;
    double imag;
} dcomplex;

typedef struct { dcomplex e[3][3]; } dsu3_matrix;

static void
llfat_init(void* act_path_coeff)
{ 
    int dev = 0;
   
    cudaSetDevice(dev); CUERR;
    
    gaugeParam.X[0] = sdim;
    gaugeParam.X[1] = sdim;
    gaugeParam.X[2] = sdim;
    gaugeParam.X[3] = tdim;

    gauge_param= & gaugeParam;
    
    setDims(gaugeParam.X);
    
    gaugeParam.blockDim = 64;
    
    gaugeParam.cpu_prec = QUDA_SINGLE_PRECISION;
    gaugeParam.cuda_prec = link_prec;
        
    size_t gSize = (gaugeParam.cpu_prec == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
    
    fatLink = malloc(4*V*gaugeSiteSize* gSize);
    if (fatLink == NULL){
	fprintf(stderr, "ERROR: malloc failed for fatLink\n");
	exit(1);
    }
    siteLink = malloc(4*V*gaugeSiteSize* gSize);
    if (siteLink == NULL){
	fprintf(stderr, "ERROR: malloc failed for sitelink\n");
	exit(1);
    }
    
    refLink = malloc(4*V*gaugeSiteSize* gSize);
    if (refLink == NULL){
	fprintf(stderr, "ERROR: malloc failed for refLink\n");
	exit(1);
    }
    
    
    createSiteLinkCPU(siteLink, gaugeParam.cpu_prec, 1);
    
#if 1
    site_link_sanity_check(siteLink, V, gaugeParam.cpu_prec, &gaugeParam);
#endif

    gaugeParam.reconstruct = link_recon;
    createLinkQuda(&cudaSiteLink, &gaugeParam);
    loadLinkToGPU(cudaSiteLink, siteLink, &gaugeParam);
    
    createStapleQuda(&cudaStaple, &gaugeParam);
    createStapleQuda(&cudaStaple1, &gaugeParam);
    
    gaugeParam.reconstruct = QUDA_RECONSTRUCT_NO;
    createLinkQuda(&cudaFatLink, &gaugeParam);
        
    initDslashCuda(cudaSiteLink);

    llfat_init_cuda(&gaugeParam, act_path_coeff); 
    
    
    return;
}

void 
llfat_end() 
{
    free(fatLink);
    free(siteLink);
    free(refLink);
    
    freeLinkQuda(&cudaSiteLink);
    freeLinkQuda(&cudaFatLink);
    freeStapleQuda(&cudaStaple);
    freeStapleQuda(&cudaStaple1);
}



static void 
llfat_test(void) 
{
    float act_path_coeff_1[6];
    double act_path_coeff_2[6];
    
    for(int i=0;i < 6;i++){
	act_path_coeff_1[i]= 0.1*i;
	act_path_coeff_2[i]= 0.1*i;
    }
    
    void* act_path_coeff;
    
    if(gaugeParam.cpu_prec == QUDA_DOUBLE_PRECISION){
	act_path_coeff = act_path_coeff_2;
    }else{
	act_path_coeff = act_path_coeff_1;	
    }
    
    llfat_init(act_path_coeff);
    
    if (verify_results){
	llfat_reference(refLink, siteLink, gaugeParam.cpu_prec, act_path_coeff);
    }
    //The number comes from CPU implementation in MILC, fermion_links_helpers.c    
    int flops= 61632; 
    
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    llfat_cuda(fatLink, siteLink, cudaFatLink, cudaSiteLink, cudaStaple, cudaStaple1, &gaugeParam, act_path_coeff);
    gettimeofday(&t1, NULL);
    double secs = t1.tv_sec - t0.tv_sec + 0.000001*(t1.tv_usec - t0.tv_usec);
    
    storeLinkToCPU(fatLink, &cudaFatLink, &gaugeParam);    
    int res;
    res = compare_floats(refLink, fatLink, 4*V*gaugeSiteSize, 1e-3, gaugeParam.cpu_prec);
    
    strong_check_link(refLink, fatLink, 4*V, gaugeParam.cpu_prec);
    
    
    printf("Test %s\n",(1 == res) ? "PASSED" : "FAILED");	    
    int volume = gaugeParam.X[0]*gaugeParam.X[1]*gaugeParam.X[2]*gaugeParam.X[3];
    double perf = 1.0* flops*volume/(secs*1024*1024*1024);
    printf("gpu time =%.2f ms, flops= %.2f Gflops\n", secs*1000, perf);

    llfat_end();
    
    if (res == 0){//failed
	printf("\n");
	printf("Warning: you test failed. \n");
	printf("	Did you use --verify?\n");
	printf("	Did you check the GPU health by running cuda memtest?\n");
    }
}            


static void
display_test_info()
{
    printf("running the following test:\n");
    
    printf("link_precision           link_reconstruct           T_dimension\n");
    printf("%s                       %s                         %d \n", 
	   get_prec_str(link_prec),
	   get_recon_str(link_recon), 
	   tdim);
    return ;
    
}

static void
usage(char** argv )
{
    printf("Usage: %s <args>\n", argv[0]);
    printf("--gprec <double/single/half> \t Link precision\n"); 
    printf("--recon <8/12> \t\t\t Long link reconstruction type\n"); 
    printf("--tdim \t\t\t\t Set T dimention size(default 24)\n"); 
    printf("--verify\n");
    printf("--help \t\t\t\t Print out this message\n"); 
    exit(1);
    return ;
}

int 
main(int argc, char **argv) 
{
    int i;
    for (i =1;i < argc; i++){
	
        if( strcmp(argv[i], "--help")== 0){
            usage(argv);
        }
	
	if( strcmp(argv[i], "--gprec") == 0){
            if (i+1 >= argc){
                usage(argv);
            }	    
	    link_prec =  get_prec(argv[i+1]);
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
	
	if( strcmp(argv[i], "--tdim") == 0){
            if (i+1 >= argc){
                usage(argv);
            }	    
	    tdim =  atoi(argv[i+1]);
	    if (tdim < 0 || tdim > 128){
		fprintf(stderr, "Error: invalid t dimention\n");
		exit(1);
	    }
            i++;
            continue;	    
        }
	
	if( strcmp(argv[i], "--sdim") == 0){
            if (i+1 >= argc){
                usage(argv);
            }	    
	    sdim =  atoi(argv[i+1]);
	    if (sdim < 0 || sdim > 128){
		fprintf(stderr, "Error: invalid space dimention\n");
		exit(1);
	    }
            i++;
            continue;	    
        }

	if( strcmp(argv[i], "--verify") == 0){
	    verify_results=1;
            continue;	    
        }	
        fprintf(stderr, "ERROR: Invalid option:%s\n", argv[i]);
        usage(argv);
    }
    
    display_test_info();
    
    llfat_test();
    
    
    return 0;
}
