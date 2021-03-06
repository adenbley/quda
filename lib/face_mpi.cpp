#include <quda_internal.h>
#include <face_quda.h>
#include <cstdio>
#include <cstdlib>
#include <quda.h>
#include <string.h>

#include <mpicomm.h>

using namespace std;

cudaStream_t *stream;

FaceBuffer::FaceBuffer(const int *X, const int nDim, const int Ninternal, 
		       const int nFace, const QudaPrecision precision) : 
  Ninternal(Ninternal), precision(precision), nDim(nDim), nFace(nFace)
{
  setupDims(X);

  // set these both = 0 `for no overlap of qmp and cudamemcpyasync
  // sendBackStrmIdx = 0, and sendFwdStrmIdx = 1 for overlap
  sendBackStrmIdx = 0;
  sendFwdStrmIdx = 1;
  recFwdStrmIdx = sendBackStrmIdx;
  recBackStrmIdx = sendFwdStrmIdx;

  nbytes = nFace*faceVolumeCB[3]*Ninternal*precision;
  if (precision == QUDA_HALF_PRECISION) nbytes += nFace*faceVolumeCB[3]*sizeof(float);
   
  cudaMallocHost((void**)&fwd_nbr_spinor_sendbuf, nbytes); CUERR;
  cudaMallocHost((void**)&back_nbr_spinor_sendbuf, nbytes); CUERR;

  if (fwd_nbr_spinor_sendbuf == NULL || back_nbr_spinor_sendbuf == NULL)
    errorQuda("malloc failed for fwd_nbr_spinor_sendbuf/back_nbr_spinor_sendbuf"); 

  cudaMallocHost((void**)&fwd_nbr_spinor, nbytes); CUERR;
  cudaMallocHost((void**)&back_nbr_spinor, nbytes); CUERR;

  if (fwd_nbr_spinor == NULL || back_nbr_spinor == NULL)
    errorQuda("malloc failed for fwd_nbr_spinor/back_nbr_spinor"); 

  pagable_fwd_nbr_spinor_sendbuf = malloc(nbytes);
  pagable_back_nbr_spinor_sendbuf = malloc(nbytes);

  if (pagable_fwd_nbr_spinor_sendbuf == NULL || pagable_back_nbr_spinor_sendbuf == NULL)
    errorQuda("malloc failed for pagable_fwd_nbr_spinor_sendbuf/pagable_back_nbr_spinor_sendbuf");
  
  pagable_fwd_nbr_spinor=malloc(nbytes);
  pagable_back_nbr_spinor=malloc(nbytes);

  if (pagable_fwd_nbr_spinor == NULL || pagable_back_nbr_spinor == NULL)
    errorQuda("malloc failed for pagable_fwd_nbr_spinor/pagable_back_nbr_spinor"); 

  return;
}

FaceBuffer::FaceBuffer(const FaceBuffer &face) {
  errorQuda("FaceBuffer copy constructor not implemented");
}

// X here is a checkboarded volume
void FaceBuffer::setupDims(const int* X)
{
  Volume = 1;
  for (int d=0; d<nDim; d++) {
    this->X[d] = X[d];
    Volume *= this->X[d];    
  }
  VolumeCB = Volume/2;

  for (int i=0; i<nDim; i++) {
    faceVolume[i] = 1;
    for (int j=0; j<nDim; j++) {
      if (i==j) continue;
      faceVolume[i] *= this->X[j];
    }
    faceVolumeCB[i] = faceVolume[i]/2;
  }
}

FaceBuffer::~FaceBuffer()
{
  if(fwd_nbr_spinor_sendbuf) cudaFreeHost(fwd_nbr_spinor_sendbuf);
  if(back_nbr_spinor_sendbuf) cudaFreeHost(back_nbr_spinor_sendbuf);
  if(fwd_nbr_spinor) cudaFreeHost(fwd_nbr_spinor);
  if(back_nbr_spinor) cudaFreeHost(back_nbr_spinor);
}

void FaceBuffer::exchangeFacesStart(cudaColorSpinorField &in, int parity,
				    int dagger, cudaStream_t *stream_p)
{
  stream = stream_p;
  
  // Prepost all receives
  recv_request1 = comm_recv(pagable_back_nbr_spinor, nbytes, BACK_NBR);
  recv_request2 = comm_recv(pagable_fwd_nbr_spinor, nbytes, FWD_NBR);

  // gather for backwards send
  in.packGhost(back_nbr_spinor_sendbuf, 3, QUDA_BACKWARDS, 
	       (QudaParity)parity, dagger, &stream[sendBackStrmIdx]); CUERR;  

  // gather for forwards send
  in.packGhost(fwd_nbr_spinor_sendbuf, 3, QUDA_FORWARDS, 
	       (QudaParity)parity, dagger, &stream[sendFwdStrmIdx]); CUERR;
}

void FaceBuffer::exchangeFacesComms() {
  cudaStreamSynchronize(stream[sendBackStrmIdx]); //required the data to be there before sending out

  memcpy(pagable_back_nbr_spinor_sendbuf, back_nbr_spinor_sendbuf, nbytes);
  send_request2 = comm_send(pagable_back_nbr_spinor_sendbuf, nbytes, BACK_NBR);

  cudaStreamSynchronize(stream[sendFwdStrmIdx]); //required the data to be there before sending out

  memcpy(pagable_fwd_nbr_spinor_sendbuf, fwd_nbr_spinor_sendbuf, nbytes);
  send_request1= comm_send(pagable_fwd_nbr_spinor_sendbuf, nbytes, FWD_NBR);
} 


void FaceBuffer::exchangeFacesWait(cudaColorSpinorField &out, int dagger)
{
  comm_wait(recv_request2);  
  comm_wait(send_request2);
  memcpy(fwd_nbr_spinor, pagable_fwd_nbr_spinor, nbytes);

  out.unpackGhost(fwd_nbr_spinor, 3, QUDA_FORWARDS,  dagger, &stream[recFwdStrmIdx]); CUERR;

  comm_wait(recv_request1);
  comm_wait(send_request1);
  memcpy(back_nbr_spinor, pagable_back_nbr_spinor, nbytes);

  out.unpackGhost(back_nbr_spinor, 3, QUDA_BACKWARDS,  dagger, &stream[recBackStrmIdx]); CUERR;
}

void FaceBuffer::exchangeCpuSpinor(cpuColorSpinorField &spinor, int oddBit, int dagger)
{

  //for all dimensions
  int len[4] = {
    nFace*faceVolumeCB[0]*Ninternal*precision,
    nFace*faceVolumeCB[1]*Ninternal*precision,
    nFace*faceVolumeCB[2]*Ninternal*precision,
    nFace*faceVolumeCB[3]*Ninternal*precision
  };

  // allocate the ghost buffer if not yet allocated
  spinor.allocateGhostBuffer();

  for(int i=0;i < 4; i++){
    spinor.packGhost(spinor.backGhostFaceSendBuffer[i], i, QUDA_BACKWARDS, (QudaParity)oddBit, dagger);
    spinor.packGhost(spinor.fwdGhostFaceSendBuffer[i], i, QUDA_FORWARDS, (QudaParity)oddBit, dagger);
  }

  unsigned long recv_request1[4], recv_request2[4];
  unsigned long send_request1[4], send_request2[4];
  int back_nbr[4] = {X_BACK_NBR, Y_BACK_NBR, Z_BACK_NBR,T_BACK_NBR};
  int fwd_nbr[4] = {X_FWD_NBR, Y_FWD_NBR, Z_FWD_NBR,T_FWD_NBR};
  int uptags[4] = {XUP, YUP, ZUP, TUP};
  int downtags[4] = {XDOWN, YDOWN, ZDOWN, TDOWN};
  
  for(int i=0;i < 4; i++){
    recv_request1[i] = comm_recv_with_tag(spinor.backGhostFaceBuffer[i], len[i], back_nbr[i], uptags[i]);
    recv_request2[i] = comm_recv_with_tag(spinor.fwdGhostFaceBuffer[i], len[i], fwd_nbr[i], downtags[i]);    
    send_request1[i]= comm_send_with_tag(spinor.fwdGhostFaceSendBuffer[i], len[i], fwd_nbr[i], uptags[i]);
    send_request2[i] = comm_send_with_tag(spinor.backGhostFaceSendBuffer[i], len[i], back_nbr[i], downtags[i]);
  }

  for(int i=0;i < 4;i++){
    comm_wait(recv_request1[i]);
    comm_wait(recv_request2[i]);
    comm_wait(send_request1[i]);
    comm_wait(send_request2[i]);
  }

}


void FaceBuffer::exchangeCpuLink(void** ghost_link, void** link_sendbuf, int nFace) {
  int uptags[4] = {XUP, YUP, ZUP,TUP};
  int fwd_nbrs[4] = {X_FWD_NBR, Y_FWD_NBR, Z_FWD_NBR, T_FWD_NBR};
  int back_nbrs[4] = {X_BACK_NBR, Y_BACK_NBR, Z_BACK_NBR, T_BACK_NBR};

  for(int dir =0; dir < 4; dir++)
    {
      int len = 2*nFace*faceVolumeCB[dir]*Ninternal;

      unsigned long recv_request = 
	comm_recv_with_tag(ghost_link[dir], len*precision, back_nbrs[dir], uptags[dir]);
      unsigned long send_request = 
	comm_send_with_tag(link_sendbuf[dir], len*precision, fwd_nbrs[dir], uptags[dir]);
      comm_wait(recv_request);
      comm_wait(send_request);
    }
}


void reduceDouble(double &sum) {

#ifdef MPI_COMMS
  comm_allreduce(&sum);
#endif

}

void reduceDoubleArray(double *sum, const int len) {

#ifdef MPI_COMMS
  comm_allreduce_array(sum, len);
#endif

}
