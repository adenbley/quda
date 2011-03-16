//
// Auto-tuned blas CUDA parameters, generated by blas_test
//

static int blas_threads[23][3] = {
  {  64,  512,   64},  // Kernel  0: copyCuda (high source precision)
  { 576,  512,  384},  // Kernel  1: copyCuda (low source precision)
  {  96,  128,  128},  // Kernel  2: axpbyCuda
  {  96,  256,  128},  // Kernel  3: xpyCuda
  {  64,  128,  128},  // Kernel  4: axpyCuda
  {  96,  128,  128},  // Kernel  5: xpayCuda
  {  96,  128,  128},  // Kernel  6: mxpyCuda
  {  96,  480,  320},  // Kernel  7: axCuda
  { 160,  128,  128},  // Kernel  8: caxpyCuda
  {  96,  128,   64},  // Kernel  9: caxpbyCuda
  {  64,   96,   96},  // Kernel 10: cxpaypbzCuda
  { 512,   64,   64},  // Kernel 11: axpyBzpcxCuda
  { 384,   64,   64},  // Kernel 12: axpyZpbxCuda
  { 480,   96,   64},  // Kernel 13: caxpbypzYmbwCuda
  { 128,  256,  256},  // Kernel 14: normCuda
  { 128,  128,  256},  // Kernel 15: reDotProductCuda
  { 256,  256,  512},  // Kernel 16: axpyNormCuda
  { 256,  256,  512},  // Kernel 17: xmyNormCuda
  { 128,  128,  512},  // Kernel 18: cDotProductCuda
  { 256,  256,  512},  // Kernel 19: xpaycDotzyCuda
  { 128,  128,  256},  // Kernel 20: cDotProductNormACuda
  { 128,  128,  128},  // Kernel 21: cDotProductNormBCuda
  { 256,  256,  512}   // Kernel 22: caxpbypzYmbwcDotProductWYNormYCuda
};

static int blas_blocks[23][3] = {
  {65536,  2048, 16384},  // Kernel  0: copyCuda (high source precision)
  { 8192,  1024, 16384},  // Kernel  1: copyCuda (low source precision)
  { 2048, 16384, 65536},  // Kernel  2: axpbyCuda
  { 2048,  8192, 65536},  // Kernel  3: xpyCuda
  { 2048, 16384, 65536},  // Kernel  4: axpyCuda
  { 2048, 16384, 32768},  // Kernel  5: xpayCuda
  { 2048, 16384, 32768},  // Kernel  6: mxpyCuda
  { 2048,  4096, 32768},  // Kernel  7: axCuda
  { 1024, 16384, 32768},  // Kernel  8: caxpyCuda
  { 2048, 65536, 32768},  // Kernel  9: caxpbyCuda
  { 4096, 65536, 32768},  // Kernel 10: cxpaypbzCuda
  {  512, 32768, 32768},  // Kernel 11: axpyBzpcxCuda
  {  512, 32768, 32768},  // Kernel 12: axpyZpbxCuda
  {  512, 65536, 65536},  // Kernel 13: caxpbypzYmbwCuda
  {   64,   256,  1024},  // Kernel 14: normCuda
  {  256,  1024,  1024},  // Kernel 15: reDotProductCuda
  {16384,    64,  4096},  // Kernel 16: axpyNormCuda
  { 4096,    64,  4096},  // Kernel 17: xmyNormCuda
  {  128,   512,  1024},  // Kernel 18: cDotProductCuda
  {  512,  1024, 16384},  // Kernel 19: xpaycDotzyCuda
  {  128,    64,   512},  // Kernel 20: cDotProductNormACuda
  {  128,   512,    64},  // Kernel 21: cDotProductNormBCuda
  {  512,   512,  1024}   // Kernel 22: caxpbypzYmbwcDotProductWYNormYCuda
};
