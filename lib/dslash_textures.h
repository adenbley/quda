#if (__CUDA_ARCH__ >= 130)
static __inline__ __device__ double2 fetch_double2(texture<int4, 1> t, int i)
{
  int4 v = tex1Dfetch(t,i);
  return make_double2(__hiloint2double(v.y, v.x), __hiloint2double(v.w, v.z));
}
#endif

// Double precision gauge field
texture<int4, 1> gauge0TexDouble;
texture<int4, 1> gauge1TexDouble;

// Single precision gauge field
texture<float4, 1, cudaReadModeElementType> gauge0TexSingle;
texture<float4, 1, cudaReadModeElementType> gauge1TexSingle;

// Half precision gauge field
texture<short4, 1, cudaReadModeNormalizedFloat> gauge0TexHalf;
texture<short4, 1, cudaReadModeNormalizedFloat> gauge1TexHalf;


texture<int4, 1> fatGauge0TexDouble;
texture<int4, 1> fatGauge1TexDouble;
texture<float2, 1, cudaReadModeElementType> fatGauge0TexSingle;
texture<float2, 1, cudaReadModeElementType> fatGauge1TexSingle;
texture<short2, 1, cudaReadModeNormalizedFloat> fatGauge0TexHalf;
texture<short2, 1, cudaReadModeNormalizedFloat> fatGauge1TexHalf;

texture<int4, 1> longGauge0TexDouble;
texture<int4, 1> longGauge1TexDouble;
texture<float4, 1, cudaReadModeElementType> longGauge0TexSingle;
texture<float4, 1, cudaReadModeElementType> longGauge1TexSingle;
texture<float2, 1, cudaReadModeElementType> longGauge0TexSingle_norecon;
texture<float2, 1, cudaReadModeElementType> longGauge1TexSingle_norecon;

texture<short4, 1, cudaReadModeNormalizedFloat> longGauge0TexHalf;
texture<short4, 1, cudaReadModeNormalizedFloat> longGauge1TexHalf;
texture<short2, 1, cudaReadModeNormalizedFloat> longGauge0TexHalf_norecon;
texture<short2, 1, cudaReadModeNormalizedFloat> longGauge1TexHalf_norecon;


// Double precision input spinor field
texture<int4, 1> spinorTexDouble;

// Single precision input spinor field
texture<float4, 1, cudaReadModeElementType> spinorTexSingle;
texture<float2, 1, cudaReadModeElementType> spinorTexSingle2;

// Half precision input spinor field
texture<short4, 1, cudaReadModeNormalizedFloat> spinorTexHalf;
texture<short2, 1, cudaReadModeNormalizedFloat> spinorTexHalf2;
texture<float, 1, cudaReadModeElementType> spinorTexNorm;

// Double precision accumulate spinor field
texture<int4, 1> accumTexDouble;

// Single precision accumulate spinor field
texture<float4, 1, cudaReadModeElementType> accumTexSingle;
texture<float2, 1, cudaReadModeElementType> accumTexSingle2;

// Half precision accumulate spinor field
texture<short4, 1, cudaReadModeNormalizedFloat> accumTexHalf;
texture<short2, 1, cudaReadModeNormalizedFloat> accumTexHalf2;
texture<float, 1, cudaReadModeElementType> accumTexNorm;

static void bindGaugeTex(const FullGauge gauge, const int oddBit, 
			 void **gauge0, void **gauge1) {
  if(oddBit) {
    *gauge0 = gauge.odd;
    *gauge1 = gauge.even;
  } else {
    *gauge0 = gauge.even;
    *gauge1 = gauge.odd;
  }
  
  if (gauge.precision == QUDA_DOUBLE_PRECISION) {
    cudaBindTexture(0, gauge0TexDouble, *gauge0, gauge.bytes); 
    cudaBindTexture(0, gauge1TexDouble, *gauge1, gauge.bytes);
  } else if (gauge.precision == QUDA_SINGLE_PRECISION) {
    cudaBindTexture(0, gauge0TexSingle, *gauge0, gauge.bytes); 
    cudaBindTexture(0, gauge1TexSingle, *gauge1, gauge.bytes);
  } else {
    cudaBindTexture(0, gauge0TexHalf, *gauge0, gauge.bytes); 
    cudaBindTexture(0, gauge1TexHalf, *gauge1, gauge.bytes);
  }
}

static void bindFatGaugeTex(const FullGauge gauge, const int oddBit, 
			    void **gauge0, void **gauge1) {
  if(oddBit) {
    *gauge0 = gauge.odd;
    *gauge1 = gauge.even;
  } else {
    *gauge0 = gauge.even;
    *gauge1 = gauge.odd;
  }
  
  if (gauge.precision == QUDA_DOUBLE_PRECISION) {
    cudaBindTexture(0, fatGauge0TexDouble, *gauge0, gauge.bytes); 
    cudaBindTexture(0, fatGauge1TexDouble, *gauge1, gauge.bytes);
  } else if (gauge.precision == QUDA_SINGLE_PRECISION) {
    cudaBindTexture(0, fatGauge0TexSingle, *gauge0, gauge.bytes); 
    cudaBindTexture(0, fatGauge1TexSingle, *gauge1, gauge.bytes);
  } else {
    cudaBindTexture(0, fatGauge0TexHalf, *gauge0, gauge.bytes); 
    cudaBindTexture(0, fatGauge1TexHalf, *gauge1, gauge.bytes);
    }
}

static void bindLongGaugeTex(const FullGauge gauge, const int oddBit, 
			    void **gauge0, void **gauge1) {
    if(oddBit) {
	*gauge0 = gauge.odd;
	*gauge1 = gauge.even;
    } else {
	*gauge0 = gauge.even;
	*gauge1 = gauge.odd;
    }
    
    if (gauge.precision == QUDA_DOUBLE_PRECISION) {
      cudaBindTexture(0, longGauge0TexDouble, *gauge0, gauge.bytes); 
      cudaBindTexture(0, longGauge1TexDouble, *gauge1, gauge.bytes);
    } else if (gauge.precision == QUDA_SINGLE_PRECISION) {
      if(gauge.reconstruct == QUDA_RECONSTRUCT_NO){ //18 reconstruct
	cudaBindTexture(0, longGauge0TexSingle_norecon, *gauge0, gauge.bytes); 
	cudaBindTexture(0, longGauge1TexSingle_norecon, *gauge1, gauge.bytes);	
      }else{
	cudaBindTexture(0, longGauge0TexSingle, *gauge0, gauge.bytes); 
	cudaBindTexture(0, longGauge1TexSingle, *gauge1, gauge.bytes);
      }
    } else {
      if(gauge.reconstruct == QUDA_RECONSTRUCT_NO){ //18 reconstruct
	cudaBindTexture(0, longGauge0TexHalf_norecon, *gauge0, gauge.bytes); 
	cudaBindTexture(0, longGauge1TexHalf_norecon, *gauge1, gauge.bytes);	
      }else{
	cudaBindTexture(0, longGauge0TexHalf, *gauge0, gauge.bytes); 
	cudaBindTexture(0, longGauge1TexHalf, *gauge1, gauge.bytes);
      }
    }
}
    

template <int N, typename spinorFloat>
  int bindSpinorTex(const int length, const spinorFloat *in, const float *inNorm,
		    const spinorFloat *x=0, const float *xNorm=0) {
  if (N==2 && sizeof(spinorFloat) == sizeof(double2)) {
    int spinor_bytes = length*sizeof(double);
    cudaBindTexture(0, spinorTexDouble, in, spinor_bytes); 
    if (x) cudaBindTexture(0, accumTexDouble, x, spinor_bytes); 
    return sizeof(double);
  } else if (N==4 && sizeof(spinorFloat) == sizeof(float4)) {
    int spinor_bytes = length*sizeof(float);
    cudaBindTexture(0, spinorTexSingle, in, spinor_bytes); 
    if (x) cudaBindTexture(0, accumTexSingle, x, spinor_bytes); 
    return sizeof(float);
  }else if  (N==2 && sizeof(spinorFloat) == sizeof(float2)){
      int spinor_bytes = length*sizeof(float);
      cudaBindTexture(0, spinorTexSingle2, in, spinor_bytes); 
      if (x) cudaBindTexture(0, accumTexSingle2, x, spinor_bytes); 
      return sizeof(float);    
  }else if (N==4 && sizeof(spinorFloat) == sizeof(short4)) {
    int spinor_bytes = length*sizeof(short);
    cudaBindTexture(0, spinorTexHalf, in, spinor_bytes); 
    if (inNorm) cudaBindTexture(0, spinorTexNorm, inNorm, spinor_bytes/12); 
    if (x) cudaBindTexture(0, accumTexHalf, x, spinor_bytes); 
    if (xNorm) cudaBindTexture(0, accumTexNorm, xNorm, spinor_bytes/12); 
    return sizeof(float);
  } else if (N==2 && sizeof(spinorFloat) == sizeof(short2)){
      int spinor_bytes = length*sizeof(short);
      cudaBindTexture(0, spinorTexHalf2, in, spinor_bytes); 
      if (inNorm) cudaBindTexture(0, spinorTexNorm, inNorm, spinor_bytes/3); 
      if (x) cudaBindTexture(0, accumTexHalf2, x, spinor_bytes); 
      if (xNorm) cudaBindTexture(0, accumTexNorm, xNorm, spinor_bytes/3); 
      return sizeof(float);
  }else {
    errorQuda("Unsupported precision and short vector type");
  }

}

template <int N, typename spinorFloat>
  int bindSpinorTex_mg(const int length, const int ghost_length, const spinorFloat *in, const float *inNorm,
		       const spinorFloat *x=0, const float *xNorm=0) {
  
  int total_length = length + ghost_length;
  
  if (N==2 && sizeof(spinorFloat) == sizeof(double2)) {
    int spinor_bytes = total_length*sizeof(double);
    cudaBindTexture(0, spinorTexDouble, in, spinor_bytes); 
    if (x) cudaBindTexture(0, accumTexDouble, x, spinor_bytes); 
    return sizeof(double);
  } else if (N==4 && sizeof(spinorFloat) == sizeof(float4)) {
    int spinor_bytes = total_length*sizeof(float);
    cudaBindTexture(0, spinorTexSingle, in, spinor_bytes); 
    if (x) cudaBindTexture(0, accumTexSingle, x, spinor_bytes); 
    return sizeof(float);
  }else if  (N==2 && sizeof(spinorFloat) == sizeof(float2)){
      int spinor_bytes = total_length*sizeof(float);
      cudaBindTexture(0, spinorTexSingle2, in, spinor_bytes); 
      if (x) cudaBindTexture(0, accumTexSingle2, x, spinor_bytes); 
      return sizeof(float);    
  }else if (N==4 && sizeof(spinorFloat) == sizeof(short4)) {
    int spinor_bytes = total_length*sizeof(short);
    cudaBindTexture(0, spinorTexHalf, in, spinor_bytes); 
    if (inNorm) cudaBindTexture(0, spinorTexNorm, inNorm, spinor_bytes/12); 
    if (x) cudaBindTexture(0, accumTexHalf, x, spinor_bytes); 
    if (xNorm) cudaBindTexture(0, accumTexNorm, xNorm, spinor_bytes/12); 
    return sizeof(float);
  } else if (N==2 && sizeof(spinorFloat) == sizeof(short2)){
      int spinor_bytes = total_length*sizeof(short);
      cudaBindTexture(0, spinorTexHalf2, in, spinor_bytes); 
      if (inNorm) cudaBindTexture(0, spinorTexNorm, inNorm, spinor_bytes/3); 
      if (x) cudaBindTexture(0, accumTexHalf2, x, spinor_bytes); 
      if (xNorm) cudaBindTexture(0, accumTexNorm, xNorm, spinor_bytes/3); 
      return sizeof(float);
  }else {
    errorQuda("Unsupported precision and short vector type");
  }

}
// Double precision clover term
texture<int4, 1> cloverTexDouble;

// Single precision clover term
texture<float4, 1, cudaReadModeElementType> cloverTexSingle;

// Half precision clover term
texture<short4, 1, cudaReadModeNormalizedFloat> cloverTexHalf;
texture<float, 1, cudaReadModeElementType> cloverTexNorm;

static QudaPrecision bindCloverTex(const FullClover clover, const int oddBit, 
				   void **cloverP, void **cloverNormP) {

  if (oddBit) {
    *cloverP = clover.odd.clover;
    *cloverNormP = clover.odd.cloverNorm;
  } else {
    *cloverP = clover.even.clover;
    *cloverNormP = clover.even.cloverNorm;
  }

  if (clover.odd.precision == QUDA_DOUBLE_PRECISION) {
    cudaBindTexture(0, cloverTexDouble, *cloverP, clover.odd.bytes); 
  } else if (clover.odd.precision == QUDA_SINGLE_PRECISION) {
    cudaBindTexture(0, cloverTexSingle, *cloverP, clover.odd.bytes); 
  } else {
    cudaBindTexture(0, cloverTexHalf, *cloverP, clover.odd.bytes); 
    cudaBindTexture(0, cloverTexNorm, *cloverNormP, clover.odd.bytes/18);
  }

  return clover.odd.precision;
}

