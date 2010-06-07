#include <color_spinor_field.h>
#include <string.h>
#include <iostream>

/*ColorSpinorField::ColorSpinorField() : init(false) {

}*/

ColorSpinorField::ColorSpinorField(const ColorSpinorParam &param) : init(false), even(0), odd(0) {
  create(param.nDim, param.x, param.nColor, param.nSpin, param.precision, param.pad, 
	 param.fieldType, param.fieldSubset, param.subsetOrder, param.fieldOrder, param.basis, 
	 param.parity);
}

ColorSpinorField::ColorSpinorField(const ColorSpinorField &field) : init(false), even(0), odd(0) {
  create(field.nDim, field.x, field.nColor, field.nSpin, field.precision, field.pad,
	 field.type, field.subset, field.subset_order, field.order, field.basis, field.parity);
}

ColorSpinorField::~ColorSpinorField() {
  destroy();
}

void ColorSpinorField::create(int Ndim, const int *X, int Nc, int Ns, QudaPrecision Prec, 
			      int Pad, FieldType Type, FieldSubset Subset, 
			      SubsetOrder Subset_order, QudaColorSpinorOrder Order,
			      GammaBasis Basis, QudaParity Parity) {
  if (Ndim > QUDA_MAX_DIM){
    errorQuda("Number of dimensions nDim = %d too great", Ndim);
  }
  nDim = Ndim;
  nColor = Nc;
  nSpin = Ns;

  precision = Prec;
  volume = 1;
  for (int d=0; d<nDim; d++) {
    x[d] = X[d];
    volume *= x[d];
  }
  pad = Pad;
  
    
  if (Subset == QUDA_FULL_FIELD_SUBSET) {
    stride = volume/2 + pad; // padding is based on half volume
    length = 2*stride*nColor*nSpin*2;
    //FIXME
    printf("ERROR: full cuda spinor is not supported for ghost spinor reason\n");
    exit(1);
  } else {
    stride = volume + pad;
    length = stride*nColor*nSpin*2;
  }

  real_length = volume*nColor*nSpin*2;

  //FIXME: only works for parity spinor
  //X[0] is already half of the full dimention
  //we need 6 faces (3 backward and 3 forward)
  
  int Vsh = X[0]*X[1]*X[2];
  ghost_length = 6*Vsh*(nColor*nSpin*2);
  bytes = (length + 6*Vsh*(nColor*nSpin*2))* precision;
  type = Type;
  subset = Subset;
  subset_order = Subset_order;
  order = Order;

  basis = Basis;

  parity = Parity;
  init = true;
}

void ColorSpinorField::destroy() {
  init = false;
}

ColorSpinorField& ColorSpinorField::operator=(const ColorSpinorField &src) {
  if (&src != this) {
    create(src.nDim, src.x, src.nColor, src.nSpin, src.precision, src.pad,
	   src.type, src.subset, src.subset_order, src.order, src.basis, src.parity);    
  }
  return *this;
}

// Resets the attributes of this field if param disagrees (and is defined)
void ColorSpinorField::reset(const ColorSpinorParam &param) {

  if (param.nColor != 0) nColor = param.nColor;
  if (param.nSpin != 0) nSpin = param.nSpin;

  if (param.precision != QUDA_INVALID_PRECISION)  precision = param.precision;
  if (param.nDim != 0) nDim = param.nDim;

  volume = 1;
  for (int d=0; d<nDim; d++) {
    if (param.x[0] != 0) x[d] = param.x[d];
    volume *= x[d];
  }
  
  if (param.pad != 0) pad = param.pad;

  if (param.fieldSubset == QUDA_FULL_FIELD_SUBSET){
    stride = volume/2 + pad;
    length = 2*stride*nColor*nSpin*2;
  }else if (param.fieldSubset == QUDA_PARITY_FIELD_SUBSET){
    stride = volume + pad;
    length = stride*nColor*nSpin*2;  
  }else{
    //do nothing, not an error
  }

  real_length = volume*nColor*nSpin*2;

  int Vsh = x[0]*x[1]*x[2];
  ghost_length = 6*Vsh*(nColor*nSpin*2);
  bytes = (length + 6*Vsh*(nColor*nSpin*2))* precision;
  //bytes = length * precision;

  if (param.fieldType != QUDA_INVALID_FIELD) type = param.fieldType;
  if (param.fieldSubset != QUDA_INVALID_SUBSET) subset = param.fieldSubset;
  if (param.subsetOrder != QUDA_INVALID_SUBSET_ORDER) subset_order = param.subsetOrder;
  if (param.fieldOrder != QUDA_INVALID_ORDER) order = param.fieldOrder;

  if (param.basis != QUDA_INVALID_BASIS) basis = param.basis;

  if (!init) errorQuda("Shouldn't be resetting a non-inited field\n");
}

// Fills the param with the contents of this field
void ColorSpinorField::fill(ColorSpinorParam &param) {
  param.nColor = nColor;
  param.nSpin = nSpin;
  param.precision = precision;
  param.nDim = nDim;
  memcpy(param.x, x, QUDA_MAX_DIM*sizeof(int));
  param.pad = pad;
  param.fieldType = type;
  param.fieldSubset = subset;
  param.subsetOrder = subset_order;
  param.fieldOrder = order;
  param.basis = basis;
  param.create = QUDA_INVALID_CREATE;
}

// For kernels with precision conversion built in
void ColorSpinorField::checkField(const ColorSpinorField &a, const ColorSpinorField &b) {
  if (a.Length() != b.Length()) {
    errorQuda("checkSpinor: lengths do not match: %d %d", a.Length(), b.Length());
  }

  if (a.Ncolor() != b.Ncolor()) {
    errorQuda("checkSpinor: colors do not match: %d %d", a.Ncolor(), b.Ncolor());
  }

  if (a.Nspin() != b.Nspin()) {
    errorQuda("checkSpinor: spins do not match: %d %d", a.Nspin(), b.Nspin());
  }
}

double norm2(const ColorSpinorField &a) {

  if (a.fieldType() == QUDA_CUDA_FIELD) {
    return normCuda(dynamic_cast<const cudaColorSpinorField&>(a));
  } else if (a.fieldType() == QUDA_CPU_FIELD) {
    return normCpu(dynamic_cast<const cpuColorSpinorField&>(a));
  } else {
    errorQuda("Field type %d not supported", a.fieldType());
  }

}
