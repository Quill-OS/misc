#include "functions.h"
extern char * device;

#define initModule(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)
#define INFO_OK 0
#define INFO_WARNING 1
#define INFO_FATAL 2

long int run_command(const char * path, const char * const arguments[], bool wait) {
	pid_t pid = -1;

#pragma GCC diagnostic   push
#pragma GCC diagnostic   ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic   ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic   ignored "-Wincompatible-pointer-types"
#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
	int status = posix_spawn(&pid, path, NULL, NULL, arguments, NULL);
#pragma GCC diagnostic pop
	if(status != 0) {
		fprintf(stderr, "Error in spawning process %s (%s)\n", path, strerror(status));
		return -(EXIT_FAILURE);
	}

	if(wait) {
		pid_t ret;
		int wstatus;
		do {
			ret = waitpid(pid, &wstatus, 0);
		} while(ret == -1 && errno == EINTR);
		if(ret != pid) {
			perror("waitpid");
			return -(EXIT_FAILURE);
		}
		else {
			if(WIFEXITED(wstatus)) {
				int exitcode = WEXITSTATUS(wstatus);
				return exitcode;
			}
			else if (WIFSIGNALED(wstatus)) {
				int sigcode = WTERMSIG(wstatus);
				psignal(sigcode, "child caught signal");
				// Help caller grok the difference between exited or terminated by a signal by using a negative return value.
				return -(sigcode);
			}
			// If the child caught a SIGSTOP because of ptrace, we're not handling it separately (i.e., we'll silently return `pid`).
			// There are facilities available to handle that, but they require greater care in how the looping is handled (c.f., waitpid(2)).
		}
	}

	return pid;
}

// https://stackoverflow.com/a/3747128/14164574
char * read_file(const char * file_path, bool strip_newline) {
	// Ensure that specified file exists, then try to read it
	if(access(file_path, F_OK) != 0) {
		return NULL;
	}

	FILE * fp = fopen(file_path , "rb");
	if(!fp) {
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	long bytes = ftell(fp);
	rewind(fp);

	// Allocate memory for entire content
	char * buffer = NULL;
	buffer = calloc(sizeof(*buffer), (size_t) bytes + 1U);
	if(!buffer) {
		goto cleanup;
	}

	// Copy the file into the buffer
	if(fread(buffer, sizeof(*buffer), (size_t) bytes, fp) < (size_t) bytes || ferror(fp) != 0) {
		// Short read or error, meep!
		free(buffer);
		buffer = NULL;
		goto cleanup;
	}

	// If requested, remove trailing newline
	if(strip_newline && bytes > 0 && buffer[bytes - 1] == '\n') {
		buffer[bytes - 1] = '\0';
	}

cleanup:
	fclose(fp);
	return buffer; // May be NULL (indicates failure)
}

// Thanks, ChatGPT ...
char * read_sysfs_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    char *buf = malloc(SYSFS_BUF_SIZE);
    if (!buf) {
        perror("malloc");
        close(fd);
        return NULL;
    }

    ssize_t nbytes = read(fd, buf, SYSFS_BUF_SIZE - 1);
    if (nbytes < 0) {
        perror("read");
        free(buf);
        close(fd);
        return NULL;
    }

    // Null-terminate the buffer
    buf[nbytes] = '\0';

    close(fd);
    return buf;
}

// https://stackoverflow.com/a/14576624/14164574
bool write_file(const char * file_path, const char * content) {
	FILE *file = fopen(file_path, "wb");
	if(!file) {
		return false;
	}

	int rc = fputs(content, file);
	fclose(file);
	return !!(rc != EOF);
}

// https://stackoverflow.com/a/39191360/14164574
bool copy_file(const char * source_file, const char * destination_file) {
	FILE * fp_I = fopen(source_file, "rb");
	if(!fp_I) {
		perror(source_file);
		return false;
	}

	FILE * fp_O = fopen(destination_file, "wb");
	if(!fp_O) {
		fclose(fp_I);
		return false;
	}

	char buffer[PIPE_BUF];
	size_t len = 0U;
	while((len = fread(buffer, sizeof(*buffer), PIPE_BUF, fp_I)) > 0) {
		fwrite(buffer, sizeof(*buffer), len, fp_O);
	}

	fclose(fp_I);
	if(fclose(fp_O) != 0) {
		return false;
	}
	return true;
}

int mkpath(const char * path, mode_t mode) {
	if(!path) {
		errno = EINVAL;
		return 1;
	}

	if(strlen(path) == 1 && path[0] == '/')
		return 0;

	mkpath(dirname(strdupa(path)), mode);

	return mkdir(path, mode);
}

// https://github.com/Kobo-InkBox/inkbox-power-daemon/blob/8296c4a1811e3921ff98e9980504c24d23435dac/src/wifi.cpp#L181-L197
int load_module(const char * module_path, const char * params) {
	int fd = open(module_path, O_RDONLY);
	if(fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}
	struct stat st;
	if(fstat(fd, &st) == -1) {
		perror("fstat");
		return EXIT_FAILURE;
	}
	size_t image_size = (size_t) st.st_size;
	void * image = malloc(image_size);
	if(!image) {
		perror("malloc");
		close(fd);
		return EXIT_FAILURE;
	}
	if(read(fd, image, image_size) != (ssize_t) image_size) {
		fprintf(stderr, "load_module %s: failed/short read\n", module_path);
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	int rc = initModule(image, image_size, params);
	free(image);
	if(rc != 0) {
		fprintf(stderr, "Couldn't init module %s\n", module_path);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

// https://stackoverflow.com/a/49334887/14164574
int set_if_flags(const char * if_name, short flags) {
	int res = 0;

	int fd = -1;
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		res = 1;
		goto cleanup;
	}

	struct ifreq ifr;
	ifr.ifr_flags = flags;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	res = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if(res < 0) {
		fprintf(stderr, "Interface '%s': Error: SIOCSIFFLAGS failed: %s\n", if_name, strerror(errno));
	}
	else {
		printf("Interface '%s': flags set to %04X.\n", if_name, (unsigned int) flags);
	}

cleanup:
	close(fd);
	return res;
}

int set_if_up(const char * if_name) {
    return set_if_flags(if_name, IFF_UP);
}

// https://www.includehelp.com/c-programs/set-ip-address-in-linux.aspx
int set_if_ip_address(const char * if_name, const char * ip_address) {
	int res = 0;

	// AF_INET - to define network interface IPv4
	// Creating socket for it
	int fd = -1;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		res = 1;
		goto cleanup;
	}

	struct ifreq ifr;
	// AF_INET - to define IPv4 Address type
	ifr.ifr_addr.sa_family = AF_INET;

	// eth0 - define the ifr_name - port name
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	// Defining sockaddr_in
	struct sockaddr_in * addr;
	addr = (struct sockaddr_in*)&ifr.ifr_addr;

	// Convert IP address in correct format to write
	inet_pton(AF_INET, ip_address, &addr->sin_addr);

	// Setting IP Address using ioctl
	res = ioctl(fd, SIOCSIFADDR, &ifr);

cleanup:
	close(fd);
	return res;
}

int info(const char * message, int mode) {
	/*
	 * Modes:
	 * - 0: Normal logging (green) - INFO_OK
	 * - 1: Warning logging (yellow) - INFO_WARNING
	 * - 2: Error logging (red) - INFO_FATAL
	*/
	if(mode == 0) {
		printf("\e[1m\e[32m * \e[0m%s\n", message);
	}
	else if(mode == 1) {
		printf("\e[1m\e[33m * \e[0m%s\n", message);
	}
	else if(mode == 2) {
		printf("\e[1m\e[31m * \e[0m%s\n", message);
	}
	else {
		return 1;
	}
	return 0;
}

int get_brightness(int mode) {
	int brightness;
	if(mode == 0) {
		if(MATCH(device, "n613")) {
			brightness = (int)read_sysfs_file("/data/config/03-brightness/config");
		}
		else if(MATCH(device, "n236") || MATCH(device, "n437")) {
			brightness = (int)read_sysfs_file("/sys/class/backlight/mxc_msp430_fl.0/brightness");
		}
		else if(MATCH(device, "n249")) {
			brightness = (int)read_sysfs_file("/sys/class/backlight/backlight_cold/actual_brightness");
		}
		else if(MATCH(device, "n418")) {
			brightness = round((float)atof(read_sysfs_file("/sys/class/leds/aw99703-bl_FL2/brightness"))/2047*100);
		}
		else {
			brightness = (int)read_sysfs_file("/sys/class/backlight/mxc_msp430.0/brightness");
		}
	}
	else {
		if(MATCH(device, "n249")) {
			brightness = (int)read_sysfs_file("/sys/class/backlight/backlight_warm/actual_brightness");
		}
		else if(MATCH(device, "n418")) {
			brightness = round((float)atof(read_sysfs_file("/sys/class/leds/aw99703-bl_FL1/brightness"))/2047*100);
		}
	}
	return brightness;
}

void set_brightness_ntxio(int value) {
	// Thanks to Kevin Short for this (GloLight)
	int light;
	if((light = open("/dev/ntx_io", O_RDWR)) == -1) {
		fprintf(stderr, "Error opening ntx_io device\n");
	}
	ioctl(light, 241, value);
	close(light);
}

void set_brightness(int brightness, int mode) {
	char brightness_char[10];
	if(MATCH(device, "n418")) {
		brightness = round((float)brightness/100*2047);
	}
	sprintf(brightness_char, "%d\n", brightness);
	if(mode == 0) {
		if(MATCH(device, "n613")) {
			set_brightness_ntxio(brightness);
		}
		else if(MATCH(device, "n249")) {
			write_file("/sys/class/backlight/backlight_cold/brightness", brightness_char);
		}
		else if(MATCH(device, "n418")) {
			write_file("/sys/class/leds/aw99703-bl_FL2/brightness", brightness_char);
		}
		else {
			write_file("/sys/class/backlight/mxc_msp430.0/brightness", brightness_char);
		}
	}
	else {
		if(MATCH(device, "n249")) {
			write_file("/sys/class/backlight/backlight_warm/brightness", brightness_char);
		}
		else if(MATCH(device, "n418")) {
			write_file("/sys/class/leds/aw99703-bl_FL1/brightness", brightness_char);
		}
	}
}
