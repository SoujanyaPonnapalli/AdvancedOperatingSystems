#define _GNU_SOURCE 
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
 #include <sys/wait.h>
#include <assert.h>
// #include <asm/cachectl.h>

#define CACHE_LINE_SIZE 64
int opt_random_access;
int bool_compete_for_memory;
int test_file_fd;
int pid;
int fd_cache_read_access, fd_cache_read_miss, fd_tlb_read_access, fd_tlb_read_miss, 
    fd_cache_write_access, fd_cache_write_miss, fd_tlb_write_access, fd_tlb_write_miss;

///////////////////////////////////////////////////////////////
// Simple, fast random number generator, here so we can observe it using profiler
long x = 1, y = 4, z = 7, w = 13;

long simplerand(void) {
	long t = x;
	t ^= t << 11;
	t ^= t >> 8;
	x = y;
	y = z;
	z = w;
	w ^= w >> 19;
	w ^= t;
	return w;
}

long x1 = 1, y11 = 4, z1 = 7, w1 = 13;

// function for randomizing even the writes and reads in do_mem_access
long simplerandZeroOne(void) {
  long t1 = x1;
  t1 ^= t1 << 11;
  t1 ^= t1 >> 8;
  x1 = y11;
  y11 = z1;
  z1 = w1;
  w1 ^= w1 >> 19;
  w1 ^= t1;
  return w1%2;
}


// p points to a region that is 1GB (ideally)
void do_mem_access(char* p, int size) {
  int i, j, count, outer, locality;
   int ws_base = 0;
   int max_base = ((size / CACHE_LINE_SIZE) - 512);
  for(outer = 0; outer < (1<<20); ++outer) {
      long r = simplerand() % max_base;
      // Pick a starting offset
      if( opt_random_access ) {
         ws_base = r;
      } else {
         ws_base += 512;
         if( ws_base >= max_base ) {
            ws_base = 0;
         }
      }
      for(locality = 0; locality < 16; locality++) {
         volatile char *a;
         char c;
         for(i = 0; i < 512; i++) {
            // Working set of 512 cache lines, 32KB
            a = p + ws_base + i * CACHE_LINE_SIZE;
            if((i%8) == 0) {
               *a = 1;
            } else {
               c = *a;
            }
         }
      }
   }
}

// versionized do_mem_access

// void do_mem_access(char* p, int size) {
//   int i, j, count, outer, locality;
//    int ws_base = 0;
//    int max_base = ((size / CACHE_LINE_SIZE) - 512);
//   for(outer = 0; outer < (1<<20); ++outer) {
//       opt_random_access = simplerandZeroOne();
//       long r = simplerand() % max_base;
//       // Pick a starting offset
//       if( opt_random_access ) {
//          ws_base = r;
//       } else {
//          ws_base += 512;
//          if( ws_base >= max_base ) {
//             ws_base = 0;
//          }
//       }
//       for(locality = 0; locality < 16; locality++) {
//          volatile char *a;
//          char c;
//          for(i = 0; i < 512; i++) {
//             // Working set of 512 cache lines, 32KB
//            if(opt_random_access) {
//              a = p + (simplerand() % max_base) + i*CACHE_LINE_SIZE;
//              opt_random_access = simplerandZeroOne();
//            }
//            else
//              a = p + ws_base + i * CACHE_LINE_SIZE;
//            if((i%8) == 0) {
//              *a = 1;
//            } else {
//              c = *a;
//            }
//          }
//       }
//    }
// }

long get_mem_size(){
  long mem_size = sysconf (_SC_AVPHYS_PAGES);
  return mem_size;
}

int compete_for_memory() {
   long mem_size = get_mem_size();
   int page_sz = sysconf(_SC_PAGE_SIZE);
   fflush(stdout);
   char* p = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                  MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS, -1, (off_t) 0);
   if (p == MAP_FAILED)
      perror("Failed anon MMAP competition");

   int i = 0;
   while(1) {
      volatile char *a;
      long r = simplerand() % (mem_size/page_sz);
      char c;
      if( i >= mem_size/page_sz ) {
         i = 0;
      }
      // One read and write per page
      //a = p + i * page_sz; // sequential access
      a = p + r * page_sz;
      c += *a;
      if((i%8) == 0) {
         *a = 1;
      }
      i++;
   }
   return 0;
}

void printCounters() {
  long long unsigned read_l1_access_count, read_l1_miss_count, read_tlb_access_count, read_tlb_miss_count, write_l1_access_count, write_l1_miss_count, write_tlb_access_count, write_tlb_miss_count; 
  assert(read(fd_cache_read_access, &read_l1_access_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_cache_read_miss, &read_l1_miss_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_tlb_read_access, &read_tlb_access_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_tlb_read_miss, &read_tlb_miss_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_cache_write_access, &write_l1_access_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_cache_write_miss, &write_l1_miss_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_tlb_write_access, &write_tlb_access_count, sizeof(long long unsigned))!=-1);
  assert(read(fd_tlb_write_miss, &write_tlb_miss_count, sizeof(long long unsigned))!=-1);

  printf("READ cacheL1 Access Count : %llu\n", read_l1_access_count);
  printf("READ cacheL1 Miss Count : %llu\n", read_l1_miss_count);
  printf("READ dataTLB Access Count : %llu\n", read_tlb_access_count);
  printf("READ dataTLB Miss Count : %llu\n", read_tlb_miss_count);
  printf("WRITE cacheL1 Access Count : %llu\n", write_l1_access_count);
  printf("WRITE cacheL1 Miss Count : %llu\n", write_l1_miss_count);
  printf("WRITE dataTLB Access Count : %llu\n", write_tlb_access_count);
  printf("WRITE dataTLB Miss Count : %llu\n", write_tlb_miss_count);   

  float val = (100*(read_l1_miss_count+0.00))/read_l1_access_count;
  printf("READ L1/L1 : %f\n", val);
  val = (100*(read_tlb_miss_count+0.00))/read_l1_access_count;
  printf("READ TLB/L1 : %f\n", val);
  val = (100*(write_l1_miss_count+0.00))/write_l1_access_count;
  printf("WRITE L1/L1 : %f\n", val);
  val = (100*(write_tlb_miss_count+0.00))/write_l1_access_count;
  printf("WRITE TLB/L1 : %f\n", val);
  
}

 
void checkReturnValue(int retValue) {
  if (retValue < 0) {
    printf("%s, %d\n", strerror(errno), errno);
    exit(EXIT_FAILURE); 
  }
}


void close_file_descriptors(){
  assert(close(fd_cache_read_access) != -1);
  assert(close(fd_cache_read_miss) != -1);
  assert(close(fd_tlb_read_access) != -1);
  assert(close(fd_tlb_read_miss) != -1);
  assert(close(fd_cache_write_access) != -1); 
  assert(close(fd_cache_write_miss) != -1);
  assert(close(fd_tlb_write_access) != -1);
  assert(close(fd_tlb_write_miss) != -1);
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  int ret;
  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

__u64 make_config_value( __u64 perf_hw_cache_id, __u64 perf_hw_cache_op_id, __u64 perf_hw_cache_op_result_id) {
  return (perf_hw_cache_id) | (perf_hw_cache_op_id << 8) | (perf_hw_cache_op_result_id << 16);
}

struct perf_event_attr * getPerfEvent(__u64 perf_hw_cache_id, __u64 perf_hw_cache_op_id, __u64 perf_hw_cache_op_result_id) {
  struct perf_event_attr *attr;
  attr = malloc(sizeof(struct perf_event_attr));
  attr->type = PERF_TYPE_HW_CACHE;
  attr->size = sizeof(struct perf_event_attr);
  attr->config = make_config_value(perf_hw_cache_id, perf_hw_cache_op_id, perf_hw_cache_op_result_id);
  return attr;
}

void make_ioctl_calls(unsigned long flag) {
  assert(ioctl(fd_cache_read_access, flag, 0) != -1);
  assert(ioctl(fd_cache_read_miss, flag, 0) != -1);
  assert(ioctl(fd_tlb_read_access, flag, 0) != -1);
  assert(ioctl(fd_tlb_read_miss, flag, 0) != -1);
  assert(ioctl(fd_cache_write_access, flag, 0) != -1);
  assert(ioctl(fd_cache_write_miss, flag, 0) != -1);
  assert(ioctl(fd_tlb_write_access, flag, 0) != -1);
  assert(ioctl(fd_tlb_write_miss, flag, 0) != -1);
}

char * get_mmaped_address(int mmap_config){
  long unsigned size = 1024*1024*1024;
  char* buff;
  if (mmap_config & MAP_ANONYMOUS )
    buff = mmap(NULL, size, PROT_READ | PROT_WRITE, mmap_config, -1, 0);
  else {
    test_file_fd = open("testFile.txt", O_CREAT | O_RDWR);
    posix_fallocate(test_file_fd, 0, size);
    if(test_file_fd < 0) {
      perror("file not opened, exiting\n");
      exit(EXIT_FAILURE);
    }
    buff = mmap(0, size, PROT_READ | PROT_WRITE, mmap_config, test_file_fd, 0);
  }
  assert(buff != MAP_FAILED);
  assert(memset(buff, '\0', size));
  assert(msync(buff, size, MS_SYNC) != -1);
  return buff;
}

static void setup_perf_counters( int mmap_config) {

  // Creating perf event structures and setting up attributes
  struct perf_event_attr* attr_cache_read_access = getPerfEvent(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS); 
  struct perf_event_attr* attr_cache_read_miss = getPerfEvent( PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
  struct perf_event_attr* attr_tlb_read_access = getPerfEvent(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS); 
  struct perf_event_attr* attr_tlb_read_miss = getPerfEvent(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
  struct perf_event_attr* attr_cache_write_access = getPerfEvent( PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS); 
  struct perf_event_attr* attr_cache_write_miss = getPerfEvent(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_MISS); 
  struct perf_event_attr* attr_tlb_write_access = getPerfEvent(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS); 
  struct perf_event_attr* attr_tlb_write_miss = getPerfEvent(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_MISS);

  // Calling perf_event_open and getting file descriptors for future use
  fd_cache_read_access = perf_event_open(attr_cache_read_access, 0, -1, -1, 0);
  fd_cache_read_miss = perf_event_open(attr_cache_read_miss, 0, -1, -1, 0);
  fd_tlb_read_access = perf_event_open(attr_tlb_read_access, 0, -1, -1, 0);
  fd_tlb_read_miss = perf_event_open(attr_tlb_read_miss, 0, -1, -1, 0);
  fd_cache_write_access = perf_event_open(attr_cache_write_access, 0, -1, -1, 0);
  fd_cache_write_miss = perf_event_open(attr_cache_write_miss, 0, -1, -1, 0);
  fd_tlb_write_access = perf_event_open(attr_tlb_write_access, 0, -1, -1, 0);
  fd_tlb_write_miss = perf_event_open(attr_tlb_write_miss, 0, -1, -1, 0);

  if (fd_cache_read_access <0 || fd_cache_read_miss < 0 || fd_tlb_read_access < 0 || fd_tlb_read_miss < 0 || 
      fd_cache_write_access <0 || fd_cache_write_miss < 0 || fd_tlb_write_access < 0 || fd_tlb_write_miss < 0 ){     
    printf("perf_event_open failed, %s, %d\n", strerror(errno), errno);
    exit(EXIT_FAILURE);
  }

  // reset perf counters;
  make_ioctl_calls(PERF_EVENT_IOC_RESET);
  
  pid = 0;
  if (bool_compete_for_memory) {
    pid = fork();
    assert(pid != -1);
  } else {
    pid = 1;
  }

  // setting the cpu affinity and flushing the cache before enabling the events
  cpu_set_t my_set;        /* Define your cpu_set bit mask. */
  CPU_ZERO(&my_set);       /* Initialize it all to 0, i.e. no CPUs selected. */
  CPU_SET(7, &my_set);     /* set the bit that represents core 7. */
  assert( sched_setaffinity(0, sizeof(cpu_set_t), &my_set) != -1); /* Set affinity of this process to */
 
 /*flushing cache*/
  int readcache, flushsize = 32*1024;
  unsigned int * cacheflush;
  cacheflush = (unsigned int *) calloc(flushsize, sizeof(unsigned int));
  for(int i=0; i<flushsize; i++){
    readcache = cacheflush[i];
  }  
  
  if ( pid != 0) {
    // Enable the counters
    make_ioctl_calls(PERF_EVENT_IOC_ENABLE);
    
    // Use mmap for allocating memory
    char * buff = get_mmaped_address(mmap_config);

    long unsigned size = 1024*1024*1024;
    do_mem_access(buff, size);
    make_ioctl_calls(PERF_EVENT_IOC_DISABLE);
    printCounters();
    close_file_descriptors();
    munmap(buff, size);    
  } 
  else if (pid == 0){

    //child process runs the compete_for_memory function
    compete_for_memory();
    exit(0);
  
  }

}

void getrusageDetails() {
  
  struct  rusage *usage;
  usage = (struct rusage*) malloc(sizeof(struct rusage));
  int ret = getrusage(RUSAGE_SELF, usage);

  printf("user CPU time used : %ld.%ld\n", usage->ru_utime.tv_sec, usage->ru_utime.tv_usec);
  printf("system CPU time used : %ld.%ld\n", usage->ru_stime.tv_sec, usage->ru_stime.tv_usec);
  printf("page reclaims (soft page faults) (ru_minflt) : %ld\n", usage->ru_minflt);
  printf("hard page faults (ru_majflt) : %ld\n", usage->ru_majflt);
  printf("swaps (ru_nswap) : %ld\n", usage->ru_nswap);
  printf("voluntary context switches (ru_nvcsw) : %ld\n", usage->ru_nvcsw);
  printf("involuntary context switches (ru_nivcsw) : %ld\n", usage->ru_nivcsw);
}


int main(int argc, char **argv) {
  if(argc < 5) {
    printf("FORMAT: ./a.out  bool1 bool2 mmap_config_shared_private mmap_config_anon_file\n");
    printf("bool1: 1 to set opt_random_access in do_mem_access, 0 otherwise\n");
    printf("bool2: 1 if a thread to compete_for_memory, 0 otherwise\n");
    printf("Config using 0 MAP_SHARED and 1 MAP_PRIVATE\n");
    printf("Config using 0 for File based 1 for MAP_ANONYMOUS\n");
    exit(EXIT_FAILURE);
  }
  if (argv[1][0] == '1')
    opt_random_access = 1;
  else 
    opt_random_access = 0;
  if (argv[2][0] == '1')
    bool_compete_for_memory = 1;
  else 
    bool_compete_for_memory = 0;

  int mmap_config = 0;
  if(argv[3][0] == '0') mmap_config |= MAP_SHARED;
  if(argv[3][0] == '1') mmap_config |= MAP_PRIVATE;
  if(argv[4][0] == '1') mmap_config |= MAP_ANONYMOUS;
	setup_perf_counters(mmap_config);
  getrusageDetails();

  if((mmap_config & MAP_ANONYMOUS) && (close(test_file_fd) < 1)) {
    perror("test file not closed! Exiting\n");
    exit(EXIT_FAILURE);
  };
  if(pid != 0) {
    kill(pid, SIGKILL);
  };

}
