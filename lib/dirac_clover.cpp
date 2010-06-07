#include <dirac.h>
#include <iostream>
#include <blas_quda.h>

DiracClover::DiracClover(const DiracParam &param)
  : DiracWilson(param), clover(*(param.clover)) {

}

DiracClover::DiracClover(const DiracClover &dirac) 
  : DiracWilson(dirac), clover(dirac.clover) {

}

DiracClover::~DiracClover() {

}

DiracClover& DiracClover::operator=(const DiracClover &dirac) {

  if (&dirac != this) {
    DiracWilson::operator=(dirac);
    clover = dirac.clover;
  }

  return *this;
}

void DiracClover::checkParitySpinor(const cudaColorSpinorField &out, const cudaColorSpinorField &in,
				    const FullClover &clover) {
  Dirac::checkParitySpinor(out, in);

  if (out.Volume() != clover.even.volume) {
    errorQuda("Spinor volume %d doesn't match even clover volume %d",
	      out.Volume(), clover.even.volume);
  }
  if (out.Volume() != clover.odd.volume) {
    errorQuda("Spinor volume %d doesn't match odd clover volume %d",
	      out.Volume(), clover.odd.volume);
  }

#if (__CUDA_ARCH__ <= 130)
  if ((clover.even.precision == QUDA_DOUBLE_PRECISION) ||
      (clover.odd.precision == QUDA_DOUBLE_PRECISION)) {
    errorQuda("Double precision not supported on this GPU");
  }
#endif

}

// Protected method, also used for applying cloverInv
void DiracClover::cloverApply(cudaColorSpinorField &out, const FullClover &clover, 
			      cudaColorSpinorField &in, const int parity) {

  if (!initDslash) initDslashConstants(gauge, in.stride, clover.even.stride);
  checkParitySpinor(in, out, clover);

  cloverCuda(out.v, out.norm, gauge, clover, in.v, in.norm, parity, 
	     in.volume, in.length, in.Precision());

  flops += 504*in.volume;
}

// Public method to apply the clover term only
void DiracClover::Clover(cudaColorSpinorField &out, cudaColorSpinorField &in, 
			 const int parity) {
  cloverApply(out, clover, in, parity);
}

// FIXME: create kernel to eliminate tmp
void DiracClover::M(cudaColorSpinorField &out,  cudaColorSpinorField &in, const QudaDagType dagger) {
  checkFullSpinor(out, in);

  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;

  bool reset = false;
  if (!tmp2) {
    tmp2 = new cudaColorSpinorField(in.Even(), param); // only create if necessary
    reset = true;
  }

  Clover(*tmp2, in.Odd(), 1);
  DslashXpay(out.Odd(), in.Even(), 1, dagger, *tmp2, -kappa);
  Clover(*tmp2, in.Even(), 0);
  DslashXpay(out.Even(), in.Odd(), 0, dagger, *tmp2, -kappa);

  if (reset) {
    delete tmp2;
    tmp2 = 0;
  }

}

void DiracClover::MdagM(cudaColorSpinorField &out, cudaColorSpinorField &in) {
  checkFullSpinor(out, in);
  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;

  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(in, param); // only create if necessary
    reset = true;
  }

  M(*tmp1, in, QUDA_DAG_NO);
  M(out, *tmp1, QUDA_DAG_YES);

  if (reset) {
    delete tmp1;
    tmp1 = 0;
  }

}

void DiracClover::Prepare(cudaColorSpinorField* &src, cudaColorSpinorField* &sol,
			  cudaColorSpinorField &x, cudaColorSpinorField &b, 
			  const QudaSolutionType solType, const QudaDagType dagger) {
  ColorSpinorParam param;

  src = &b;
  sol = &x;
}

void DiracClover::Reconstruct(cudaColorSpinorField &x, const cudaColorSpinorField &b,
			      const QudaSolutionType solType, const QudaDagType dagger) {
  // do nothing
}

DiracCloverPC::DiracCloverPC(const DiracParam &param)
  : DiracClover(param), cloverInv(*(param.cloverInv)) {

}

DiracCloverPC::DiracCloverPC(const DiracCloverPC &dirac) 
  : DiracClover(dirac), cloverInv(dirac.clover) {

}

DiracCloverPC::~DiracCloverPC() {

}

DiracCloverPC& DiracCloverPC::operator=(const DiracCloverPC &dirac) {

  if (&dirac != this) {
    DiracClover::operator=(dirac);
    cloverInv = dirac.cloverInv;
    tmp1 = dirac.tmp1;
    tmp2 = dirac.tmp2;
  }

  return *this;
}

// Public method
void DiracCloverPC::CloverInv(cudaColorSpinorField &out,  cudaColorSpinorField &in, 
			      const int parity) {
  cloverApply(out, cloverInv, in, parity);
}

// apply hopping term, then clover: (A_ee^-1 D_eo) or (A_oo^-1 D_oe),
// and likewise for dagger: (A_ee^-1 D^dagger_eo) or (A_oo^-1 D^dagger_oe)
void DiracCloverPC::Dslash(cudaColorSpinorField &out, cudaColorSpinorField &in, 
			   const int parity, const QudaDagType dagger) {

  if (!initDslash) initDslashConstants(gauge, in.stride, cloverInv.even.stride);
  checkParitySpinor(in, out, cloverInv);

  cloverDslashCuda(out.v, out.norm, gauge, cloverInv, in.v, in.norm, parity, dagger, 
		   0, 0, 0.0, out.volume, out.length, in.Precision());

  flops += (1320+504)*in.volume;
}

// xpay version of the above
void DiracCloverPC::DslashXpay(cudaColorSpinorField &out, cudaColorSpinorField &in, 
			       const int parity, const QudaDagType dagger, 
			       const cudaColorSpinorField &x, const double &k) {

  if (!initDslash) initDslashConstants(gauge, in.stride, cloverInv.even.stride);
  checkParitySpinor(in, out, cloverInv);

  cloverDslashCuda(out.v, out.norm, gauge, cloverInv, in.v, in.norm, parity, dagger, 
		   x.v, x.norm, k, out.volume, out.length, in.Precision());

  flops += (1320+504+48)*in.volume;
}

// Apply the even-odd preconditioned clover-improved Dirac operator
void DiracCloverPC::M(cudaColorSpinorField &out, cudaColorSpinorField &in, 
		      const QudaDagType dagger) {
  double kappa2 = -kappa*kappa;

  // FIXME: For asymmetric, a "DslashCxpay" kernel would improve performance.
  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;
  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(in, param); // only create if necessary
    reset = true;
  }

  if (matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC) {
    Dslash(*tmp1, in, 1, dagger);
    Clover(out, in, 0);
    DiracWilson::DslashXpay(out, *tmp1, 0, dagger, out, kappa2); // safe since out is not read after writing
  } else if (matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) {
    Dslash(*tmp1, in, 0, dagger);
    Clover(out, in, 1);
    DiracWilson::DslashXpay(out, *tmp1, 1, dagger, out, kappa2);
  } else if (!dagger) { // symmetric preconditioning
    if (matpcType == QUDA_MATPC_EVEN_EVEN) {
      Dslash(*tmp1, in, 1, dagger);
      DslashXpay(out, *tmp1, 0, dagger, in, kappa2); 
    } else if (matpcType == QUDA_MATPC_ODD_ODD) {
      Dslash(*tmp1, in, 0, dagger);
      DslashXpay(out, *tmp1, 1, dagger, in, kappa2); 
    } else {
      errorQuda("Invalid matpcType");
    }
  } else { // symmetric preconditioning, dagger
    if (matpcType == QUDA_MATPC_EVEN_EVEN) {
      CloverInv(out, in, 0); 
      Dslash(*tmp1, out, 1, dagger);
      DiracWilson::DslashXpay(out, *tmp1, 0, dagger, in, kappa2); 
    } else if (matpcType == QUDA_MATPC_ODD_ODD) {
      CloverInv(out, in, 1); 
      Dslash(*tmp1, out, 0, dagger);
      DiracWilson::DslashXpay(out, *tmp1, 1, dagger, in, kappa2); 
    } else {
      errorQuda("MatPCType %d not valid for DiracCloverPC", matpcType);
    }
  }
  
  if (reset) {
    delete tmp1;
    tmp1 = 0;
  }

}

void DiracCloverPC::MdagM(cudaColorSpinorField &out, cudaColorSpinorField &in) {
  // need extra temporary because of symmetric preconditioning dagger
  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;

  bool reset = false;
  if (!tmp2) {
    tmp2 = new cudaColorSpinorField(in, param); // only create if necessary
    reset = true;
  }

  M(*tmp2, in, QUDA_DAG_NO);
  M(out, *tmp2, QUDA_DAG_YES);

  if (reset) {
    delete tmp2;
    tmp2 = 0;
  }

}

void DiracCloverPC::Prepare(cudaColorSpinorField* &src, cudaColorSpinorField* &sol, 
			    cudaColorSpinorField &x, cudaColorSpinorField &b, 
			    const QudaSolutionType solType, const QudaDagType dagger) {


  // we desire solution to preconditioned system
  if (solType == QUDA_MATPC_SOLUTION || solType == QUDA_MATPCDAG_MATPC_SOLUTION) {
    DiracClover::Prepare(src, sol, x, b, solType, dagger);
    return;
  }

  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;
  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(b.Even(), param); // only create if necessary
    reset = true;
  }

  // we desire solution to full system
  if (matpcType == QUDA_MATPC_EVEN_EVEN) {
    // src = A_ee^-1 (b_e + k D_eo A_oo^-1 b_o)
    src = &(x.Odd());
    CloverInv(*src, b.Odd(), 1);
    DiracWilson::DslashXpay(*tmp1, *src, 0, dagger, b.Even(), kappa);
    CloverInv(*src, *tmp1, 0);
    sol = &(x.Even());
  } else if (matpcType == QUDA_MATPC_ODD_ODD) {
    // src = A_oo^-1 (b_o + k D_oe A_ee^-1 b_e)
    src = &(x.Even());
    CloverInv(*src, b.Even(), 0);
    DiracWilson::DslashXpay(*tmp1, *src, 1, dagger, b.Odd(), kappa);
    CloverInv(*src, *tmp1, 1);
    sol = &(x.Odd());
  } else if (matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC) {
    // src = b_e + k D_eo A_oo^-1 b_o
    src = &(x.Odd());
    CloverInv(*tmp1, b.Odd(), 1); // safe even when *tmp1 = b.odd
    DiracWilson::DslashXpay(*src, *tmp1, 0, dagger, b.Even(), kappa);
    sol = &(x.Even());
  } else if (matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) {
    // src = b_o + k D_oe A_ee^-1 b_e
    src = &(x.Even());
    CloverInv(*tmp1, b.Even(), 0); // safe even when *tmp1 = b.even
    DiracWilson::DslashXpay(*src, *tmp1, 1, dagger, b.Odd(), kappa);
    sol = &(x.Odd());
  } else {
    errorQuda("MatPCType %d not valid for DiracClover", matpcType);
  }

  // here we use final solution to store parity solution and parity source
  // b is now up for grabs if we want

  if (reset) {
    delete tmp1;
    tmp1 = 0;
  }

}

void DiracCloverPC::Reconstruct(cudaColorSpinorField &x, const cudaColorSpinorField &b,
				const QudaSolutionType solType, const QudaDagType dagger) {

  if (solType == QUDA_MATPC_SOLUTION || solType == QUDA_MATPCDAG_MATPC_SOLUTION) {
    return DiracClover::Reconstruct(x, b, solType, dagger);
  }

  checkFullSpinor(x, b);

  ColorSpinorParam param;
  param.create = QUDA_NULL_CREATE;
  bool reset = false;
  if (!tmp1) {
    tmp1 = new cudaColorSpinorField(b.Even(), param); // only create if necessary
    reset = true;
  }

  // create full solution

  if (matpcType == QUDA_MATPC_EVEN_EVEN ||
      matpcType == QUDA_MATPC_EVEN_EVEN_ASYMMETRIC) {
    // x_o = A_oo^-1 (b_o + k D_oe x_e)
    DiracWilson::DslashXpay(*tmp1, x.Even(), 1, dagger, b.Odd(), kappa);
    CloverInv(x.Odd(), *tmp1, 1);
  } else if (matpcType == QUDA_MATPC_ODD_ODD ||
      matpcType == QUDA_MATPC_ODD_ODD_ASYMMETRIC) {
    // x_e = A_ee^-1 (b_e + k D_eo x_o)
    DiracWilson::DslashXpay(*tmp1, x.Odd(), 0, dagger, b.Even(), kappa);
    CloverInv(x.Even(), *tmp1, 0);
  } else {
    errorQuda("MatPCType %d not valid for DiracClover", matpcType);
  }

  if (reset) {
    delete tmp1;
    tmp1 = 0;
  }

}

