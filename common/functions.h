#include <stdio.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <linux/input.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>
#include <alloca.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <spawn.h>
#include <math.h>

// Macros
#define MATCH(s1, s2) ((s1 != NULL && s2 != NULL) && strcmp(s1, s2) == 0)
#define NOT_MATCH(s1, s2) ((!s1 || !s2) || strcmp(s1, s2) != 0)
#define FILE_EXISTS(path) access(path, F_OK) == 0
#define MOUNT(s, t, f, m, d) \
({ \
        if(mount(s, t, f, m, d) != 0) { \
                perror("Failed to mount " t); \
                exit(EXIT_FAILURE); \
        } \
})
// Have to waipid the returned value at a later time to collect zombies
#define RUN(prog, ...) \
({ \
        const char * const args[] = { prog, ##__VA_ARGS__, NULL }; \
	run_command(prog, args, false); \
})
// Will wait until termination to collect the process
#define REAP(prog, ...) \
({ \
        const char * const args[] = { prog, ##__VA_ARGS__, NULL }; \
	run_command(prog, args, true); \
})
#define SYSFS_BUF_SIZE 128
#define INFO_OK 0
#define INFO_WARNING 1
#define INFO_FATAL 2

// Functions
long int run_command(const char * path, const char * const arguments[], bool wait);
char * read_file(const char * file_path, bool strip_newline);
char * read_sysfs_file(const char * path);
bool write_file(const char * file_path, const char * content);
bool copy_file(const char * source_file, const char * destination_file);
int mkpath(const char * path, mode_t mode);
int load_module(const char * module_path, const char * params);
int set_if_flags(const char * if_name, short flags);
int set_if_up(const char * if_name);
int set_if_ip_address(const char * if_name, const char * ip_address);
int info(const char * message, int mode);
void read_sector(char * buff, size_t len, const char * device_node, off_t sector, size_t sector_size);
void show_alert_splash(int error_code, bool flag);
int get_pid_by_name(const char * name);
void kill_process(const char * name);
