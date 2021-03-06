#define READ_SPINOR_DOUBLE(spinor, stride, sp_idx, norm_idx)    \
  double2 I0 = fetch_double2((spinor), sp_idx + 0*(stride));   \
  double2 I1 = fetch_double2((spinor), sp_idx + 1*(stride));   \
  double2 I2 = fetch_double2((spinor), sp_idx + 2*(stride));   \
  double2 I3 = fetch_double2((spinor), sp_idx + 3*(stride));   \
  double2 I4 = fetch_double2((spinor), sp_idx + 4*(stride));   \
  double2 I5 = fetch_double2((spinor), sp_idx + 5*(stride));   \
  double2 I6 = fetch_double2((spinor), sp_idx + 6*(stride));   \
  double2 I7 = fetch_double2((spinor), sp_idx + 7*(stride));   \
  double2 I8 = fetch_double2((spinor), sp_idx + 8*(stride));   \
  double2 I9 = fetch_double2((spinor), sp_idx + 9*(stride));   \
  double2 I10 = fetch_double2((spinor), sp_idx + 10*(stride)); \
  double2 I11 = fetch_double2((spinor), sp_idx + 11*(stride));

#define READ_SPINOR_DOUBLE_UP(spinor, stride, sp_idx, norm_idx) \
  double2 I0 = fetch_double2((spinor), sp_idx + 0*(stride));   \
  double2 I1 = fetch_double2((spinor), sp_idx + 1*(stride));   \
  double2 I2 = fetch_double2((spinor), sp_idx + 2*(stride));   \
  double2 I3 = fetch_double2((spinor), sp_idx + 3*(stride));   \
  double2 I4 = fetch_double2((spinor), sp_idx + 4*(stride));   \
  double2 I5 = fetch_double2((spinor), sp_idx + 5*(stride));

#define READ_SPINOR_DOUBLE_DOWN(spinor, stride, sp_idx, norm_idx)      \
  double2 I6 = fetch_double2((spinor), sp_idx + 6*(stride));   \
  double2 I7 = fetch_double2((spinor), sp_idx + 7*(stride));   \
  double2 I8 = fetch_double2((spinor), sp_idx + 8*(stride));   \
  double2 I9 = fetch_double2((spinor), sp_idx + 9*(stride));   \
  double2 I10 = fetch_double2((spinor), sp_idx + 10*(stride)); \
  double2 I11 = fetch_double2((spinor), sp_idx + 11*(stride));

#define READ_SPINOR_SINGLE(spinor, stride, sp_idx, norm_idx)	   \
  float4 I0 = tex1Dfetch((spinor), sp_idx + 0*(stride));   \
  float4 I1 = tex1Dfetch((spinor), sp_idx + 1*(stride));   \
  float4 I2 = tex1Dfetch((spinor), sp_idx + 2*(stride));   \
  float4 I3 = tex1Dfetch((spinor), sp_idx + 3*(stride));   \
  float4 I4 = tex1Dfetch((spinor), sp_idx + 4*(stride));   \
  float4 I5 = tex1Dfetch((spinor), sp_idx + 5*(stride));

#define READ_SPINOR_SINGLE_UP(spinor, stride, sp_idx, norm_idx)	   \
  float4 I0 = tex1Dfetch((spinor), sp_idx + 0*(stride));   \
  float4 I1 = tex1Dfetch((spinor), sp_idx + 1*(stride));   \
  float4 I2 = tex1Dfetch((spinor), sp_idx + 2*(stride));   \

#define READ_SPINOR_SINGLE_DOWN(spinor, stride, sp_idx, norm_idx)  \
  float4 I3 = tex1Dfetch((spinor), sp_idx + 3*(stride));   \
  float4 I4 = tex1Dfetch((spinor), sp_idx + 4*(stride));   \
  float4 I5 = tex1Dfetch((spinor), sp_idx + 5*(stride));

#define READ_SPINOR_HALF(spinor, stride, sp_idx, norm_idx)	   \
  float4 I0 = tex1Dfetch((spinor), sp_idx + 0*(stride));   \
  float4 I1 = tex1Dfetch((spinor), sp_idx + 1*(stride));   \
  float4 I2 = tex1Dfetch((spinor), sp_idx + 2*(stride));   \
  float4 I3 = tex1Dfetch((spinor), sp_idx + 3*(stride));   \
  float4 I4 = tex1Dfetch((spinor), sp_idx + 4*(stride));   \
  float4 I5 = tex1Dfetch((spinor), sp_idx + 5*(stride));   \
  float C = tex1Dfetch((spinorTexNorm), norm_idx);	      \
  I0.x *= C; I0.y *= C;	I0.z *= C; I0.w *= C;	              \
  I1.x *= C; I1.y *= C;	I1.z *= C; I1.w *= C;	              \
  I2.x *= C; I2.y *= C;	I2.z *= C; I2.w *= C;                 \
  I3.x *= C; I3.y *= C;	I3.z *= C; I3.w *= C;	              \
  I4.x *= C; I4.y *= C; I4.z *= C; I4.w *= C;	              \
  I5.x *= C; I5.y *= C;	I5.z *= C; I5.w *= C;					     

#define READ_SPINOR_HALF_UP(spinor, stride, sp_idx, norm_idx)	      \
  float4 I0 = tex1Dfetch((spinor), sp_idx + 0*(stride));      \
  float4 I1 = tex1Dfetch((spinor), sp_idx + 1*(stride));      \
  float4 I2 = tex1Dfetch((spinor), sp_idx + 2*(stride));      \
  float C = tex1Dfetch((spinorTexNorm), norm_idx);	      \
  I0.x *= C; I0.y *= C;	I0.z *= C; I0.w *= C;	              \
  I1.x *= C; I1.y *= C;	I1.z *= C; I1.w *= C;	              \
  I2.x *= C; I2.y *= C;	I2.z *= C; I2.w *= C;                 \

#define READ_SPINOR_HALF_DOWN(spinor, stride, sp_idx, norm_idx)	      \
  float4 I3 = tex1Dfetch((spinor), sp_idx + 3*(stride));      \
  float4 I4 = tex1Dfetch((spinor), sp_idx + 4*(stride));      \
  float4 I5 = tex1Dfetch((spinor), sp_idx + 5*(stride));      \
  float C = tex1Dfetch((spinorTexNorm), norm_idx);	      \
  I3.x *= C; I3.y *= C;	I3.z *= C; I3.w *= C;	              \
  I4.x *= C; I4.y *= C; I4.z *= C; I4.w *= C;	              \
  I5.x *= C; I5.y *= C;	I5.z *= C; I5.w *= C;					     

#define READ_ACCUM_DOUBLE(spinor, stride)			\
  double2 accum0 = fetch_double2((spinor), sid + 0*(stride));   \
  double2 accum1 = fetch_double2((spinor), sid + 1*(stride));   \
  double2 accum2 = fetch_double2((spinor), sid + 2*(stride));   \
  double2 accum3 = fetch_double2((spinor), sid + 3*(stride));   \
  double2 accum4 = fetch_double2((spinor), sid + 4*(stride));   \
  double2 accum5 = fetch_double2((spinor), sid + 5*(stride));   \
  double2 accum6 = fetch_double2((spinor), sid + 6*(stride));   \
  double2 accum7 = fetch_double2((spinor), sid + 7*(stride));   \
  double2 accum8 = fetch_double2((spinor), sid + 8*(stride));   \
  double2 accum9 = fetch_double2((spinor), sid + 9*(stride));   \
  double2 accum10 = fetch_double2((spinor), sid + 10*(stride)); \
  double2 accum11 = fetch_double2((spinor), sid + 11*(stride));	

#define READ_ACCUM_SINGLE(spinor, stride)			\
  float4 accum0 = tex1Dfetch((spinor), sid + 0*(stride));       \
  float4 accum1 = tex1Dfetch((spinor), sid + 1*(stride));       \
  float4 accum2 = tex1Dfetch((spinor), sid + 2*(stride));       \
  float4 accum3 = tex1Dfetch((spinor), sid + 3*(stride));       \
  float4 accum4 = tex1Dfetch((spinor), sid + 4*(stride));       \
  float4 accum5 = tex1Dfetch((spinor), sid + 5*(stride)); 

#define READ_ACCUM_HALF(spinor, stride)				   \
  float4 accum0 = tex1Dfetch((spinor), sid + 0*(stride));	   \
  float4 accum1 = tex1Dfetch((spinor), sid + 1*(stride));	   \
  float4 accum2 = tex1Dfetch((spinor), sid + 2*(stride));	   \
  float4 accum3 = tex1Dfetch((spinor), sid + 3*(stride));	   \
  float4 accum4 = tex1Dfetch((spinor), sid + 4*(stride));	   \
  float4 accum5 = tex1Dfetch((spinor), sid + 5*(stride));	   \
  float C = tex1Dfetch((accumTexNorm), sid);		           \
  accum0.x *= C; accum0.y *= C;	accum0.z *= C; accum0.w *= C;      \
  accum1.x *= C; accum1.y *= C;	accum1.z *= C; accum1.w *= C;      \
  accum2.x *= C; accum2.y *= C;	accum2.z *= C; accum2.w *= C;      \
  accum3.x *= C; accum3.y *= C;	accum3.z *= C; accum3.w *= C;      \
  accum4.x *= C; accum4.y *= C; accum4.z *= C; accum4.w *= C;      \
  accum5.x *= C; accum5.y *= C;	accum5.z *= C; accum5.w *= C;					     


#define WRITE_SPINOR_DOUBLE2(stride)			   \
  out[0*(stride)+sid] = make_double2(o00_re, o00_im);	   \
  out[1*(stride)+sid] = make_double2(o01_re, o01_im);	   \
  out[2*(stride)+sid] = make_double2(o02_re, o02_im);	   \
  out[3*(stride)+sid] = make_double2(o10_re, o10_im);	   \
  out[4*(stride)+sid] = make_double2(o11_re, o11_im);	   \
  out[5*(stride)+sid] = make_double2(o12_re, o12_im);	   \
  out[6*(stride)+sid] = make_double2(o20_re, o20_im);	   \
  out[7*(stride)+sid] = make_double2(o21_re, o21_im);	   \
  out[8*(stride)+sid] = make_double2(o22_re, o22_im);	   \
  out[9*(stride)+sid] = make_double2(o30_re, o30_im);	   \
  out[10*(stride)+sid] = make_double2(o31_re, o31_im);	   \
  out[11*(stride)+sid] = make_double2(o32_re, o32_im);		 

#define WRITE_SPINOR_FLOAT4(stride)				     \
  out[0*(stride)+sid] = make_float4(o00_re, o00_im, o01_re, o01_im); \
  out[1*(stride)+sid] = make_float4(o02_re, o02_im, o10_re, o10_im); \
  out[2*(stride)+sid] = make_float4(o11_re, o11_im, o12_re, o12_im); \
  out[3*(stride)+sid] = make_float4(o20_re, o20_im, o21_re, o21_im); \
  out[4*(stride)+sid] = make_float4(o22_re, o22_im, o30_re, o30_im); \
  out[5*(stride)+sid] = make_float4(o31_re, o31_im, o32_re, o32_im);

#define WRITE_SPINOR_SHORT4(stride)					\
  float c0 = fmaxf(fabsf(o00_re), fabsf(o00_im));			\
  float c1 = fmaxf(fabsf(o01_re), fabsf(o02_im));			\
  float c2 = fmaxf(fabsf(o02_re), fabsf(o01_im));			\
  float c3 = fmaxf(fabsf(o10_re), fabsf(o10_im));			\
  float c4 = fmaxf(fabsf(o11_re), fabsf(o11_im));			\
  float c5 = fmaxf(fabsf(o12_re), fabsf(o12_im));			\
  float c6 = fmaxf(fabsf(o20_re), fabsf(o20_im));			\
  float c7 = fmaxf(fabsf(o21_re), fabsf(o21_im));			\
  float c8 = fmaxf(fabsf(o22_re), fabsf(o22_im));			\
  float c9 = fmaxf(fabsf(o30_re), fabsf(o30_im));			\
  float c10 = fmaxf(fabsf(o31_re), fabsf(o31_im));			\
  float c11 = fmaxf(fabsf(o32_re), fabsf(o32_im));			\
  c0 = fmaxf(c0, c1);							\
  c1 = fmaxf(c2, c3);							\
  c2 = fmaxf(c4, c5);							\
  c3 = fmaxf(c6, c7);							\
  c4 = fmaxf(c8, c9);							\
  c5 = fmaxf(c10, c11);							\
  c0 = fmaxf(c0, c1);							\
  c1 = fmaxf(c2, c3);							\
  c2 = fmaxf(c4, c5);							\
  c0 = fmaxf(c0, c1);							\
  c0 = fmaxf(c0, c2);							\
  outNorm[sid] = c0;							\
  float scale = __fdividef(MAX_SHORT, c0);				\
  o00_re *= scale; o00_im *= scale; o01_re *= scale; o01_im *= scale;	\
  o02_re *= scale; o02_im *= scale; o10_re *= scale; o10_im *= scale;	\
  o11_re *= scale; o11_im *= scale; o12_re *= scale; o12_im *= scale;	\
  o20_re *= scale; o20_im *= scale; o21_re *= scale; o21_im *= scale;	\
  o22_re *= scale; o22_im *= scale; o30_re *= scale; o30_im *= scale;	\
  o31_re *= scale; o31_im *= scale; o32_re *= scale; o32_im *= scale;	\
  out[sid+0*(stride)] = make_short4((short)o00_re, (short)o00_im, (short)o01_re, (short)o01_im); \
  out[sid+1*(stride)] = make_short4((short)o02_re, (short)o02_im, (short)o10_re, (short)o10_im); \
  out[sid+2*(stride)] = make_short4((short)o11_re, (short)o11_im, (short)o12_re, (short)o12_im); \
  out[sid+3*(stride)] = make_short4((short)o20_re, (short)o20_im, (short)o21_re, (short)o21_im); \
  out[sid+4*(stride)] = make_short4((short)o22_re, (short)o22_im, (short)o30_re, (short)o30_im); \
  out[sid+5*(stride)] = make_short4((short)o31_re, (short)o31_im, (short)o32_re, (short)o32_im);

/************* the following is used by staggered *****************/
#if 0

#define SHORT_LENGTH 65536
#define SCALE_FLOAT ((SHORT_LENGTH-1) * 0.5)
#define SHIFT_FLOAT (-1.f / (SHORT_LENGTH-1))
#define REVERSE_SCALE_FLOAT (3.05180438e-5f)
//#define short2float(a) ( __fdividef(a, SCALE_FLOAT) - SHIFT_FLOAT)
#define short2float(a) (a*REVERSE_SCALE_FLOAT + 1.52590219e-5f)


#ifndef DIRECT_ACCESS_SPINOR //spinor access control

#define READ_1ST_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 I0 = tex1Dfetch((spinor), idx + 0*mystride);		\
  float2 I1 = tex1Dfetch((spinor), idx + 1*mystride);		\
  float2 I2 = tex1Dfetch((spinor), idx + 2*mystride);

#define READ_3RD_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 T0 = tex1Dfetch((spinor), idx + 0*mystride);		\
  float2 T1 = tex1Dfetch((spinor), idx + 1*mystride);		\
  float2 T2 = tex1Dfetch((spinor), idx + 2*mystride);

#define READ_1ST_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 I0 = fetch_double2((spinor), idx + 0*mystride);	\
  double2 I1 = fetch_double2((spinor), idx + 1*mystride);	\
  double2 I2 = fetch_double2((spinor), idx + 2*mystride);

#define READ_3RD_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 T0 = fetch_double2((spinor), idx + 0*mystride);	\
  double2 T1 = fetch_double2((spinor), idx + 1*mystride);	\
  double2 T2 = fetch_double2((spinor), idx + 2*mystride);


#define READ_1ST_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 I0 = tex1Dfetch((spinor), idx + 0*mystride);			\
  float2 I1 = tex1Dfetch((spinor), idx + 1*mystride);			\
  float2 I2 = tex1Dfetch((spinor), idx + 2*mystride);			\
  {float C = tex1Dfetch((spinorTexNorm), idx);				\
    I0.x *= C; I0.y *= C;						\
    I1.x *= C; I1.y *= C;						\
    I2.x *= C; I2.y *= C;}

#define READ_3RD_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 T0 = tex1Dfetch((spinor), idx + 0*mystride);			\
  float2 T1 = tex1Dfetch((spinor), idx + 1*mystride);			\
  float2 T2 = tex1Dfetch((spinor), idx + 2*mystride);			\
  {float C = tex1Dfetch((spinorTexNorm), idx);				\
    T0.x *= C; T0.y *= C;						\
    T1.x *= C; T1.y *= C;						\
    T2.x *= C; T2.y *= C;}


#define READ_ST_ACCUM_HALF(spinor, stride)		\
  float2 accum0 = tex1Dfetch((spinor), sid + 0*stride);	\
  float2 accum1 = tex1Dfetch((spinor), sid + 1*stride);	\
  float2 accum2 = tex1Dfetch((spinor), sid + 2*stride);	\
  float C = tex1Dfetch((accumTexNorm), sid);			\
  accum0.x *= C; accum0.y *= C;					\
  accum1.x *= C; accum1.y *= C;					\
  accum2.x *= C; accum2.y *= C;       

#else //spinor access control

#define READ_1ST_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 I0 = spinor[idx + 0*mystride];				\
  float2 I1 = spinor[idx + 1*mystride];				\
  float2 I2 = spinor[idx + 2*mystride];

#define READ_3RD_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 T0 = spinor[idx + 0*mystride];				\
  float2 T1 = spinor[idx + 1*mystride];				\
  float2 T2 = spinor[idx + 2*mystride];

#define READ_1ST_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 I0 = spinor[idx + 0*mystride];			\
  double2 I1 = spinor[idx + 1*mystride];			\
  double2 I2 = spinor[idx + 2*mystride];

#define READ_3RD_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 T0 = spinor[idx + 0*mystride];			\
  double2 T1 = spinor[idx + 1*mystride];			\
  double2 T2 = spinor[idx + 2*mystride];

#define READ_1ST_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 I0, I1, I2;							\
  {									\
    short2 S0 = in[idx + 0*mystride];					\
    short2 S1 = in[idx + 1*mystride];					\
    short2 S2 = in[idx + 2*mystride];					\
    float C = inNorm[idx];						\
    I0.x =C*short2float(S0.x); I0.y =C*short2float(S0.y);		\
    I1.x =C*short2float(S1.x); I1.y =C*short2float(S1.y);		\
    I2.x =C*short2float(S2.x); I2.y =C*short2float(S2.y);		\
  }

#define READ_3RD_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 T0, T1, T2;							\
  {									\
    short2 S0 = in[idx + 0*mystride];					\
    short2 S1 = in[idx + 1*mystride];					\
    short2 S2 = in[idx + 2*mystride];					\
    float C = inNorm[idx];						\
    T0.x =C*short2float(S0.x); T0.y =C*short2float(S0.y);		\
    T1.x =C*short2float(S1.x); T1.y =C*short2float(S1.y);		\
    T2.x =C*short2float(S2.x); T2.y =C*short2float(S2.y);		\
  }


#define READ_ST_ACCUM_HALF(spinor, stride)				\
  float2 accum0, accum1, accum2;					\
  {									\
    short2 S0 = x[sid + 0*stride];					\
    short2 S1 = x[sid + 1*stride];					\
    short2 S2 = x[sid + 2*stride];					\
    float C = xNorm[sid];						\
    accum0.x =C*short2float(S0.x); accum0.y =C*short2float(S0.y);	\
    accum1.x =C*short2float(S1.x); accum1.y =C*short2float(S1.y);	\
    accum2.x =C*short2float(S2.x); accum2.y =C*short2float(S2.y);	\
  }

#endif //spinor access control




#define WRITE_ST_SPINOR_DOUBLE2()				\
  g_out[0*sp_stride+sid] = make_double2(o00_re, o00_im);	\
  g_out[1*sp_stride+sid] = make_double2(o01_re, o01_im);	\
  g_out[2*sp_stride+sid] = make_double2(o02_re, o02_im);

#define WRITE_ST_SPINOR_FLOAT2()			\
  g_out[0*sp_stride+sid] = make_float2(o00_re, o00_im);	\
  g_out[1*sp_stride+sid] = make_float2(o01_re, o01_im);	\
  g_out[2*sp_stride+sid] = make_float2(o02_re, o02_im);

#define WRITE_ST_SPINOR_SHORT2()					\
  float c0 = fmaxf(fabsf(o00_re), fabsf(o00_im));			\
  float c1 = fmaxf(fabsf(o01_re), fabsf(o01_im));			\
  float c2 = fmaxf(fabsf(o02_re), fabsf(o02_im));			\
  c0 = fmaxf(c0, c1);							\
  c0 = fmaxf(c0, c2);							\
  outNorm[sid] = c0;							\
  float scale = __fdividef(MAX_SHORT, c0);				\
  o00_re *= scale; o00_im *= scale; o01_re *= scale; o01_im *= scale;	\
  o02_re *= scale; o02_im *= scale;					\
  g_out[sid+0*sp_stride] = make_short2((short)o00_re, (short)o00_im);	\
  g_out[sid+1*sp_stride] = make_short2((short)o01_re, (short)o01_im);	\
  g_out[sid+2*sp_stride] = make_short2((short)o02_re, (short)o02_im);

#define READ_AND_SUM_ST_SPINOR()					\
  o00_re += g_out[0*sp_stride+sid].x; o00_im += g_out[0*sp_stride+sid].y; \
  o01_re += g_out[1*sp_stride+sid].x; o01_im += g_out[1*sp_stride+sid].y; \
  o02_re += g_out[2*sp_stride+sid].x; o02_im += g_out[2*sp_stride+sid].y; \
  


#define READ_AND_SUM_ST_SPINOR_HALF()			\
  float C = outNorm[sid];				\
  o00_re += C*short2float(g_out[0*sp_stride + sid].x);	\
  o00_im += C*short2float(g_out[0*sp_stride + sid].y);	\
  o01_re += C*short2float(g_out[1*sp_stride + sid].x);	\
  o01_im += C*short2float(g_out[1*sp_stride + sid].y);	\
  o02_re += C*short2float(g_out[2*sp_stride + sid].x);	\
  o02_im += C*short2float(g_out[2*sp_stride + sid].y);	
  
#define READ_ST_ACCUM_SINGLE(spinor, stride)		\
  float2 accum0 = tex1Dfetch((spinor), sid + 0*stride);	\
  float2 accum1 = tex1Dfetch((spinor), sid + 1*stride);	\
  float2 accum2 = tex1Dfetch((spinor), sid + 2*stride);     

#define READ_ST_SPINOR_HALF(spinor)				\
  float2 I0 = tex1Dfetch((spinor), sp_idx + 0*sp_stride);	\
  float2 I1 = tex1Dfetch((spinor), sp_idx + 1*sp_stride);	\
  float2 I2 = tex1Dfetch((spinor), sp_idx + 2*sp_stride);	\
  float C = tex1Dfetch((spinorTexNorm), sp_idx);		\
  I0.x *= C; I0.y *= C;						\
  I1.x *= C; I1.y *= C;						\
  I2.x *= C; I2.y *= C;                                  


#endif




#ifndef DIRECT_ACCESS_SPINOR //spinor access control

#define READ_1ST_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 I0 = tex1Dfetch((spinor), idx + 0*mystride);		\
  float2 I1 = tex1Dfetch((spinor), idx + 1*mystride);		\
  float2 I2 = tex1Dfetch((spinor), idx + 2*mystride);

#define READ_3RD_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 T0 = tex1Dfetch((spinor), idx + 0*mystride);		\
  float2 T1 = tex1Dfetch((spinor), idx + 1*mystride);		\
  float2 T2 = tex1Dfetch((spinor), idx + 2*mystride);

#define READ_1ST_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 I0 = fetch_double2((spinor), idx + 0*mystride);	\
  double2 I1 = fetch_double2((spinor), idx + 1*mystride);	\
  double2 I2 = fetch_double2((spinor), idx + 2*mystride);

#define READ_3RD_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 T0 = fetch_double2((spinor), idx + 0*mystride);	\
  double2 T1 = fetch_double2((spinor), idx + 1*mystride);	\
  double2 T2 = fetch_double2((spinor), idx + 2*mystride);

#else //spinor access control

#define READ_1ST_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 I0 = spinor[idx + 0*mystride];				\
  float2 I1 = spinor[idx + 1*mystride];				\
  float2 I2 = spinor[idx + 2*mystride];

#define READ_3RD_NBR_SPINOR_SINGLE(spinor, idx, mystride)	\
  float2 T0 = spinor[idx + 0*mystride];				\
  float2 T1 = spinor[idx + 1*mystride];				\
  float2 T2 = spinor[idx + 2*mystride];

#define READ_1ST_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 I0 = spinor[idx + 0*mystride];			\
  double2 I1 = spinor[idx + 1*mystride];			\
  double2 I2 = spinor[idx + 2*mystride];

#define READ_3RD_NBR_SPINOR_DOUBLE(spinor, idx, mystride)	\
  double2 T0 = spinor[idx + 0*mystride];			\
  double2 T1 = spinor[idx + 1*mystride];			\
  double2 T2 = spinor[idx + 2*mystride];

#endif //spinor access control

#define READ_1ST_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 I0 = tex1Dfetch((spinor), idx + 0*mystride);			\
  float2 I1 = tex1Dfetch((spinor), idx + 1*mystride);			\
  float2 I2 = tex1Dfetch((spinor), idx + 2*mystride);			\
  {  int moffset = 0;							\
    if (idx >=(3*sp_stride +3*(3*Vsh))){					\
      moffset = 2*sp_stride + 6*Vsh;					\
    }else if (idx >= (3*sp_stride)){					\
      moffset = 2*sp_stride ;						\
    }									\
    float C = tex1Dfetch((spinorTexNorm), idx - moffset);		\
    I0.x *= C; I0.y *= C;						\
    I1.x *= C; I1.y *= C;						\
    I2.x *= C; I2.y *= C;}

#define READ_3RD_NBR_SPINOR_HALF(spinor, idx, mystride)			\
  float2 T0 = tex1Dfetch((spinor), idx + 0*mystride);			\
  float2 T1 = tex1Dfetch((spinor), idx + 1*mystride);			\
  float2 T2 = tex1Dfetch((spinor), idx + 2*mystride);			\
  { int moffset = 0;							\
    if (idx >=(3*sp_stride +3*(3*Vsh) )){				\
      moffset = 2*sp_stride + 6*Vsh;					\
    }else if (idx >= (3*sp_stride)){					\
      moffset = 2*sp_stride ;						\
    }									\
    float C = tex1Dfetch((spinorTexNorm), idx-moffset);			\
    T0.x *= C; T0.y *= C;						\
    T1.x *= C; T1.y *= C;						\
    T2.x *= C; T2.y *= C;}


#define WRITE_ST_SPINOR_DOUBLE2()				\
  g_out[0*sp_stride+sid] = make_double2(o00_re, o00_im);	\
  g_out[1*sp_stride+sid] = make_double2(o01_re, o01_im);	\
  g_out[2*sp_stride+sid] = make_double2(o02_re, o02_im);

#define WRITE_ST_SPINOR_FLOAT2()			\
  g_out[0*sp_stride+sid] = make_float2(o00_re, o00_im);	\
  g_out[1*sp_stride+sid] = make_float2(o01_re, o01_im);	\
  g_out[2*sp_stride+sid] = make_float2(o02_re, o02_im);

#define WRITE_ST_SPINOR_SHORT2()					\
  float c0 = fmaxf(fabsf(o00_re), fabsf(o00_im));			\
  float c1 = fmaxf(fabsf(o01_re), fabsf(o01_im));			\
  float c2 = fmaxf(fabsf(o02_re), fabsf(o02_im));			\
  c0 = fmaxf(c0, c1);							\
  c0 = fmaxf(c0, c2);							\
  outNorm[sid] = c0;							\
  float scale = __fdividef(MAX_SHORT, c0);				\
  o00_re *= scale; o00_im *= scale; o01_re *= scale; o01_im *= scale;	\
  o02_re *= scale; o02_im *= scale;					\
  g_out[sid+0*sp_stride] = make_short2((short)o00_re, (short)o00_im);	\
  g_out[sid+1*sp_stride] = make_short2((short)o01_re, (short)o01_im);	\
  g_out[sid+2*sp_stride] = make_short2((short)o02_re, (short)o02_im);

#define READ_AND_SUM_ST_SPINOR()					\
  o00_re += g_out[0*sp_stride+sid].x; o00_im += g_out[0*sp_stride+sid].y; \
  o01_re += g_out[1*sp_stride+sid].x; o01_im += g_out[1*sp_stride+sid].y; \
  o02_re += g_out[2*sp_stride+sid].x; o02_im += g_out[2*sp_stride+sid].y; \
  
#define SHORT_LENGTH 65536
#define SCALE_FLOAT ((SHORT_LENGTH-1) * 0.5)
#define SHIFT_FLOAT (-1.f / (SHORT_LENGTH-1))
#define short2float(a) ( __fdividef(a, SCALE_FLOAT) - SHIFT_FLOAT)

#define READ_AND_SUM_ST_SPINOR_HALF()			\
  float C = outNorm[sid];				\
  o00_re += C*short2float(g_out[0*sp_stride + sid].x);	\
  o00_im += C*short2float(g_out[0*sp_stride + sid].y);	\
  o01_re += C*short2float(g_out[1*sp_stride + sid].x);	\
  o01_im += C*short2float(g_out[1*sp_stride + sid].y);	\
  o02_re += C*short2float(g_out[2*sp_stride + sid].x);	\
  o02_im += C*short2float(g_out[2*sp_stride + sid].y);	
  
#define READ_ST_ACCUM_SINGLE(spinor)				\
  float2 accum0 = tex1Dfetch((spinor), sid + 0*sp_stride);	\
  float2 accum1 = tex1Dfetch((spinor), sid + 1*sp_stride);	\
  float2 accum2 = tex1Dfetch((spinor), sid + 2*sp_stride);     

#define READ_ST_SPINOR_HALF(spinor)				\
  float2 I0 = tex1Dfetch((spinor), sp_idx + 0*sp_stride);	\
  float2 I1 = tex1Dfetch((spinor), sp_idx + 1*sp_stride);	\
  float2 I2 = tex1Dfetch((spinor), sp_idx + 2*sp_stride);	\
  float C = tex1Dfetch((spinorTexNorm), sp_idx);		\
  I0.x *= C; I0.y *= C;						\
  I1.x *= C; I1.y *= C;						\
  I2.x *= C; I2.y *= C;                                  


#define READ_ST_ACCUM_HALF(spinor)				\
  float2 accum0 = tex1Dfetch((spinor), sid + 0*sp_stride);	\
  float2 accum1 = tex1Dfetch((spinor), sid + 1*sp_stride);	\
  float2 accum2 = tex1Dfetch((spinor), sid + 2*sp_stride);	\
  float C = tex1Dfetch((accumTexNorm), sid);			\
  accum0.x *= C; accum0.y *= C;					\
  accum1.x *= C; accum1.y *= C;					\
  accum2.x *= C; accum2.y *= C;       

#define READ_ST_ACCUM_DOUBLE(spinor)				   \
  double2 accum0 = fetch_double2((spinor), sid + 0*(sp_stride));   \
  double2 accum1 = fetch_double2((spinor), sid + 1*(sp_stride));   \
  double2 accum2 = fetch_double2((spinor), sid + 2*(sp_stride));   
