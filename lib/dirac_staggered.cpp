#include <dirac.h>
#include <blas_quda.h>
#include <exchange_face.h>
#include <sys/time.h>
#include "mpicomm.h"


DiracStaggered::DiracStaggered(const DiracParam &param)
    : Dirac(param), fatGauge(param.fatGauge), longGauge(param.longGauge) {

}

DiracStaggered::DiracStaggered(const DiracStaggered &dirac) 
  : Dirac(dirac) {

}

DiracStaggered::~DiracStaggered() {

}

DiracStaggered& DiracStaggered::operator=(const DiracStaggered &dirac) {

  if (&dirac != this) {
    Dirac::operator=(dirac);
    tmp1=dirac.tmp1;
  }
 
  return *this;
}



void DiracStaggered::checkParitySpinor(const cudaColorSpinorField &in, const cudaColorSpinorField &out) {

  if (in.Precision() != out.Precision()) {
    errorQuda("Input and output spinor precisions don't match in dslash_quda");
  }

  if (in.Stride() != out.Stride()) {
    errorQuda("Input %d and output %d spinor strides don't match in dslash_quda", in.Stride(), out.Stride());
  }

  if (in.fieldSubset() != QUDA_PARITY_FIELD_SUBSET || out.fieldSubset() != QUDA_PARITY_FIELD_SUBSET) {
    errorQuda("ColorSpinorFields are not single parity, in = %d, out = %d", 
	      in.fieldSubset(), out.fieldSubset());
  }

  if ((out.Volume() != 2*fatGauge->volume && out.fieldSubset() == QUDA_FULL_FIELD_SUBSET) ||
      (out.Volume() != fatGauge->volume && out.fieldSubset() == QUDA_PARITY_FIELD_SUBSET) ) {
      errorQuda("Spinor volume %d doesn't match gauge volume %d", out.Volume(), fatGauge->volume);
  }

}


void DiracStaggered::Dslash(cudaColorSpinorField &out, cudaColorSpinorField &in, 
			 const int parity, const QudaDagType dagger) {

  if (!initDslash) {
    initDslashConstants(*fatGauge, in.Stride(), 0);
    initStaggeredDslashConstants(*fatGauge, *longGauge);
  }
  checkParitySpinor(in, out);
  
  staggeredDslashCuda(out.v, out.norm, *fatGauge, *longGauge, &in, parity, dagger, 
		      0, 0, 0, out.volume, out.length, out.ghost_length, in.Precision());
  
  flops += 1187*in.volume;
}

void DiracStaggered::DslashXpay(cudaColorSpinorField &out, cudaColorSpinorField &in, 
				const int parity, const QudaDagType dagger,
				const cudaColorSpinorField &x, const double &k) {
    
    if (!initDslash){
	initDslashConstants(*fatGauge, in.Stride(), 0);
	initStaggeredDslashConstants(*fatGauge, *longGauge);
    }
    checkParitySpinor(in, out);
    
  staggeredDslashCuda(out.v, out.norm, *fatGauge, *longGauge, &in, parity, dagger, x.v, x.norm, k, 
		      out.volume, out.length, out.ghost_length, in.Precision());
  flops += (1187+12)*in.volume;
}

void DiracStaggered::M(cudaColorSpinorField &out, cudaColorSpinorField &in, const QudaDagType dagger) 
{
  double mkappa2 = - kappa*kappa;
  if (!initDslash){
    initDslashConstants(*fatGauge, in.Stride(), 0);
    initStaggeredDslashConstants(*fatGauge, *longGauge);
  }
  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;
  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(in, param); // only create if necessary
    reset = true;
  }
  
  int parity= 1;    
  Dslash(*tmp1, in, parity, dagger);
  
  parity =0;
  DslashXpay(out, *tmp1, parity, dagger, in, mkappa2);
  
  if (reset) {
    delete tmp1;
    tmp1 = 0;
  }
}

void DiracStaggered::MdagM(cudaColorSpinorField &out, cudaColorSpinorField &in) 
{
  
  if (!initDslash){
    initDslashConstants(*fatGauge, in.Stride(), 0);
    initStaggeredDslashConstants(*fatGauge, *longGauge);
  }
  
  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;
  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(in, param); // only create if necessary
    reset = false;
  }
  
  QudaParity parity= in.qudaParity();    
  QudaParity other_parity;
  if (parity == QUDA_EVEN_PARITY){
    other_parity = QUDA_ODD_PARITY;
  }else if (parity == QUDA_ODD_PARITY){
    other_parity = QUDA_EVEN_PARITY;
  }else if (parity == QUDA_FULL_PARITY){
    //nothing
  }else{
    errorQuda("ERROR: invalid parity(%d) in function\n", parity);

  }

  QudaDagType dagger = QUDA_DAG_NO;
  
  if (parity != QUDA_FULL_PARITY){
    Dslash(*tmp1, in, other_parity, dagger);  
    DslashXpay(out, *tmp1, parity, dagger, in, 4*mass*mass);
  }else{

    cudaColorSpinorField* mytmp = dynamic_cast<cudaColorSpinorField*>(tmp1->even);
    cudaColorSpinorField* ineven = dynamic_cast<cudaColorSpinorField*>(in.even);
    cudaColorSpinorField* inodd = dynamic_cast<cudaColorSpinorField*>(in.odd);
    cudaColorSpinorField* outeven = dynamic_cast<cudaColorSpinorField*>(out.even);
    cudaColorSpinorField* outodd = dynamic_cast<cudaColorSpinorField*>(out.odd);
    
    //even
    Dslash(*mytmp, *ineven, QUDA_ODD_PARITY, dagger);  
    DslashXpay(*outeven, *mytmp, QUDA_EVEN_PARITY, dagger, *ineven, 4*mass*mass);
    
    //odd
    Dslash(*mytmp, *inodd, QUDA_EVEN_PARITY, dagger);  
    DslashXpay(*outodd, *mytmp, QUDA_ODD_PARITY, dagger, *inodd, 4*mass*mass);    
  }
  if (reset) {
      delete tmp1;
      tmp1 = 0;
  }
}

void DiracStaggered::Prepare(cudaColorSpinorField* &src, cudaColorSpinorField* &sol,
			  cudaColorSpinorField &x, cudaColorSpinorField &b, 
			  const QudaSolutionType solutionType, const QudaDagType dagger) {
  ColorSpinorParam param;
  
  src = &b;
  sol = &x;
}

void DiracStaggered::Reconstruct(cudaColorSpinorField &x, const cudaColorSpinorField &b,
			      const QudaSolutionType solutionType, const QudaDagType dagger) {
  // do nothing
}

