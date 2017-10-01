#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

int main() {
	int fd, ret, count = 10000;
	char * buf;

	struct  rusage *usage;
	usage = (struct rusage *) malloc (sizeof(struct rusage));

	buf = (char *) malloc(sizeof(char)*(count+2));

	fd = open("/proc/self/maps",O_RDONLY);
	
	if (fd < 0) {
		perror("Unable to open /proc/self/maps");
		exit(-1);
	}

	ret = read(fd, buf, count);

	if (ret < 0) {
		perror("Couldnt read /proc/self/maps");
		exit(-1);
	}
	// print the contents of the /proc/self/maps
	printf("%s\n", buf);

	// getrusage
	ret = getrusage(RUSAGE_SELF, usage);

	printf("user CPU time used %d.%ld\n", usage->ru_utime.tv_sec, usage->ru_utime.tv_usec);
	printf("system CPU time used %d.%ld\n", usage->ru_stime.tv_sec, usage->ru_stime.tv_usec);


	printf("resident set size (ru_maxrss) : %ld\n", usage->ru_maxrss);
	printf("integral shared memory  size (ru_ixrss) : %ld\n", usage->ru_ixrss);
	printf("integral unshared data size (ru_idrss) : %ld\n", usage->ru_idrss);
	printf("integral unshared stack size (ru_isrss) : %ld\n", usage->ru_isrss);
	printf("page reclaims (soft page faults) (ru_minflt) : %ld\n", usage->ru_minflt);
	printf("hard page faults (ru_majflt) : %ld\n", usage->ru_majflt);
	printf("swaps (ru_nswap) : %ld\n", usage->ru_nswap);
	printf("block input operations (ru_inblock) : %ld\n", usage->ru_inblock);
	printf("block output operations (ru_oublock) : %ld\n", usage->ru_oublock);
	printf("ipc messages sent (ru_msgsnd) : %ld\n", usage->ru_msgsnd);
	printf("ipc messages received (ru_msgrcv) : %ld\n", usage->ru_msgrcv);
	printf("signals received (ru_nsignals) : %ld\n", usage->ru_nsignals);
	printf("voluntary context switches (ru_nvcsw) : %ld\n", usage->ru_nvcsw);
	printf("involuntary context switches (ru_nivcsw) : %ld\n", usage->ru_nivcsw);
	

	free(buf);
	close(fd);
	return  0;
}
