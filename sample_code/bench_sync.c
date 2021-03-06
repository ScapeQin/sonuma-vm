/*
 * Scale-Out NUMA Open Source License
 *
 * Copyright (c) 2017, Parallel Systems Architecture Lab, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Parallel Systems Architecture Lab, EPFL,
 *   nor the names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LAB,
 * EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Remote read test for libsonuma and SoftRMC
 */

#include <vector>
#include <algorithm>

#include "sonuma.h"

#define ITERS 4096
#define SLOT_SIZE 64
#define OBJ_READ_SIZE 64
#define CTX_0 0
#define CPU_FREQ 2.4

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi)<<32) ;
}

int main(int argc, char **argv)
{
  rmc_wq_t *wq;
  rmc_cq_t *cq;

  int num_iter = (int)ITERS;

  if (argc != 3) {
    fprintf(stdout,"Usage: ./bench_sync <target_nid> <op_type>\n"); 
    return 1;
  }
    
  int snid = atoi(argv[1]);
  char op = *argv[2];
  uint64_t ctx_size = PAGE_SIZE * PAGE_SIZE;
  uint64_t buf_size = PAGE_SIZE;

  uint8_t *ctx = NULL;
  uint8_t *lbuff = NULL;
  uint64_t lbuff_slot;
  uint64_t ctx_offset;
  
  int fd = kal_open((char*)RMC_DEV);  
  if(fd < 0) {
    printf("cannot open RMC dev. driver\n");
    return -1;
  }

  //register local buffer
  if(kal_reg_lbuff(fd, &lbuff, buf_size/PAGE_SIZE) < 0) {
    printf("Failed to allocate local buffer\n");
    return -1;
  } else {
    fprintf(stdout, "Local buffer was mapped to address %p, number of pages is %d\n",
	    lbuff, buf_size/PAGE_SIZE);
  }

  //register context
  if(kal_reg_ctx(fd, &ctx, ctx_size/PAGE_SIZE) < 0) {
    printf("Failed to allocate context\n");
    return -1;
  } else {
    fprintf(stdout, "Ctx buffer was registered, ctx_size=%d, %d pages.\n",
	    ctx_size, ctx_size*sizeof(uint8_t) / PAGE_SIZE);
  }

  //register WQ
  if(kal_reg_wq(fd, &wq) < 0) {
    printf("Failed to register WQ\n");
    return -1;
  } else {
    fprintf(stdout, "WQ was registered.\n");
  }

  //register CQ
  if(kal_reg_cq(fd, &cq) < 0) {
    printf("Failed to register CQ\n");
  } else {
    fprintf(stdout, "CQ was registered.\n");
  }
  
  fprintf(stdout,"Init done! Will execute %d WQ operations - SYNC! (snid = %d)\n",
	  num_iter, snid);

  unsigned long long start, end;
  
  lbuff_slot = 0;
  
  for(size_t i = 0; i < num_iter; i++) {
    ctx_offset = (i * PAGE_SIZE) % ctx_size;
    lbuff_slot = (i * sizeof(uint32_t)) % (PAGE_SIZE - OBJ_READ_SIZE);

    start = rdtsc();

    if(op == 'r') {
      rmc_rread_sync(wq, cq, lbuff, lbuff_slot, snid, CTX_0, ctx_offset, OBJ_READ_SIZE);
    } else if(op == 'w') {
      rmc_rwrite_sync(wq, cq, lbuff, lbuff_slot, snid, CTX_0, ctx_offset, OBJ_READ_SIZE);
    } else
      ;
    
    end = rdtsc();
    
    if(op == 'r') {
      printf("read this number: %u\n", ((uint32_t*)lbuff)[lbuff_slot/sizeof(uint32_t)]);
    }
    printf("time to execute this op: %lf ns\n", ((double)end - start)/CPU_FREQ);
  }
 
  return 0;
}
