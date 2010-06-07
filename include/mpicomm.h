#ifndef __MPICOMM_H__
#define __MPICOMM_H_

#define BACK_NBR 1
#define FWD_NBR 2


void		comm_init(int, char**);
int		comm_rank(void);
int		comm_size(void);
unsigned long	comm_send(void*, int, int);
unsigned long	comm_recv(void*, int, int);
void		comm_wait(unsigned long);
void		comm_allreduce(double* data);
void		comm_barrier(void);
void		comm_exit(int);
void		comm_cleanup(void);
int		comm_gpuid();

extern int verbose;

#define PRINTF(fmt,...) do{						\
    if (verbose){							\
      printf("[%d]"fmt, comm_rank(), ##__VA_ARGS__);			\
    }else{								\
      if (comm_rank()==0){						\
	printf("[%d]"fmt, comm_rank(), ##__VA_ARGS__);			\
      }									\
    }									\
  }while(0)	


#endif
   

