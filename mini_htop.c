#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <termios.h>
#include <fcntl.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[0m"
#define PAD "  "

typedef struct
{
	long user, nice, system, idle, iowait, irq, softirq;
} CPUInfo;

typedef struct
{
	long total;
	long av;
	float usage;
} MemInfo;

typedef struct
{
	int h;
	int m;
	int s;
} TimeInfo;

typedef struct
{
	char id[128];
	char name[512];
	long mem;
} ProcInfo;

#define BUFFER_SIZE 8192

static char buff[BUFFER_SIZE];
static int curr_pos = 0;

void bprintf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	curr_pos += vsnprintf(buff + curr_pos, sizeof(buff) - curr_pos, fmt, args);
	va_end(args);
}

void bflush()
{
	printf("\033[H\033[2J");
	fwrite(buff, 1, curr_pos, stdout);
	fflush(stdout);
	curr_pos = 0; // reset buffer
}

void enableRawMode()
{
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag &= ~(ICANON | ECHO); // disable line buffering and echo
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // non-blocking reads
}

void restore_terminal()
{
	struct termios t;
	tcgetattr(STDIN_FILENO, &t);
	t.c_lflag |= (ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
	printf("\033[H\033[2J");
}

float get_cpu_usage()
{
	static CPUInfo prev_cpu = {0};
	FILE *f;

	f = fopen("/proc/stat", "r");

	CPUInfo cpu;

	fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld", &cpu.user, &cpu.nice, &cpu.system, &cpu.idle, &cpu.iowait, &cpu.irq, &cpu.softirq);

	long current_active_time = cpu.user + cpu.nice + cpu.system + cpu.irq + cpu.softirq;
	long previous_active_time = prev_cpu.user + prev_cpu.nice + prev_cpu.system + prev_cpu.irq + prev_cpu.softirq;
	long diff_active = current_active_time - previous_active_time;

	long curr_idle = cpu.idle + cpu.iowait;
	long prev_idle = prev_cpu.idle + prev_cpu.iowait;
	long diff_idle = curr_idle - prev_idle;

	prev_cpu = cpu;
	fclose(f);

	if (diff_idle == 0.)
		return 0.;
	return (float)(diff_active) / (diff_active + diff_idle) * 100;
}

MemInfo get_mem_usage()
{
	FILE *f;

	f = fopen("/proc/meminfo", "r");
	MemInfo mem = {0};

	char freeline[256];

	fscanf(f, "MemTotal: %ld", &mem.total);
	while (fgets(freeline, sizeof(freeline), f))
		if (fscanf(f, "MemAvailable: %ld", &mem.av))
			break;
	mem.usage = (float)(mem.av) / mem.total * 100;

	fclose(f);
	return mem;
}

int get_process_count()
{
	DIR *dir = opendir("/proc");
	struct dirent *entry;
	int count = 0;

	while ((entry = readdir(dir)) != NULL)
	{
		// d_name[0] is a digit = it's a PID directory
		if (isdigit(entry->d_name[0]))
			count++;
	}

	closedir(dir);
	return count;
}

TimeInfo get_uptime()
{
	FILE *f;

	f = fopen("/proc/uptime", "r");

	TimeInfo tinfo;

	float total;

	fscanf(f, "%f", &total);

	tinfo.s = (int)total % 60;

	total /= 60;
	tinfo.m = (int)total % 60;

	total /= 60;
	tinfo.h = (int)total;

	fclose(f);

	return tinfo;
}

void clear_screen()
{
	// bprintf("\033[2J");
	bprintf("\033[H");
}

void print_slider(float val, int width)
{
	bprintf("[");
	if (val > 66)
	{
		bprintf(RED);
	}
	else if (val > 33)
	{
		bprintf(YELLOW);
	}
	else
	{
		bprintf(GREEN);
	}
	for (int i = 0; i < width; i++)
	{
		if (i < width * val / 100)
		{
			bprintf("#");
		}
		else
		{
			bprintf(WHITE "—");
		}
	}
	bprintf("]");
}

void print_cpu()
{

	float usage = get_cpu_usage();
	bprintf(PAD "%-17s", GREEN "CPU" WHITE ": ");
	print_slider(usage, 25);
	bprintf("   %2.1f\%\n", usage);
}

void print_mem()
{

	MemInfo mem = get_mem_usage();
	bprintf(PAD "%-17s", GREEN "Memory" WHITE ": ");
	print_slider(mem.usage, 25);
	bprintf("  (%.1fGB / %.1fGB)\n", mem.av / 1000000., mem.total / 1000000.);
}

void print_proccess_count(int count)
{
	bprintf(PAD "%-17s", GREEN "Proc Count" WHITE ": ");
	bprintf("%d\n", count);
}

void print_uptime()
{
	TimeInfo tinfo = get_uptime();
	bprintf(PAD "%-17s", GREEN "Uptime" WHITE ": ");
	bprintf("%dH %02dM %02dS\n", tinfo.h, tinfo.m, tinfo.s);
}

void print_proc_headers()
{
	bprintf("\n\n");
	bprintf(PAD CYAN "PID        NAME                               MemUsage\n");
	bprintf(PAD WHITE "__________ __________________________________ ______________\n");
}

void bubblesort(ProcInfo *processes, int count)
{
	for (int i = 0; i < count - 1; i++)
	{
		for (int j = 0; j < count - i - 1; j++)
		{
			if (processes[j].mem < processes[j + 1].mem)
			{
				ProcInfo temp = processes[j];
				processes[j] = processes[j + 1];
				processes[j + 1] = temp;
			}
		}
	}
}
void print_proccess()
{
	DIR *dir = opendir("/proc");
	struct dirent *entry;
	int count = 0;

	int proccount = get_process_count();
	ProcInfo *proccesses = malloc(sizeof(ProcInfo) * proccount);

	while ((entry = readdir(dir)) != NULL)
	{
		// d_name[0] is a digit = it's a PID directory
		if (isdigit(entry->d_name[0]))
		{

			ProcInfo proc;

			strncpy(proc.id, entry->d_name, sizeof(proc.id));

			FILE *cmdf;

			char cmdlinepath[256];
			snprintf(cmdlinepath, sizeof(cmdlinepath), "/proc/%s/cmdline", entry->d_name);
			cmdf = fopen(cmdlinepath, "r");

			if (!cmdf)
			{
				proccount--;
				continue;
			}
			char procname[34];

			fgets(procname, sizeof(procname), cmdf);

			fclose(cmdf);
			strncpy(proc.name, procname, sizeof(proc.name));
			char *space = strchr(proc.name, ' ');
			if (space)
				*space = '\0';

			FILE *memf;

			char statuspath[256];
			snprintf(statuspath, sizeof(statuspath), "/proc/%s/status", entry->d_name);
			memf = fopen(statuspath, "r");

			if (!memf)
			{
				proccount--;
				continue;
			}
			char memline[256];

			while (fgets(memline, sizeof(memline), memf))
			{
				if (fscanf(memf, "VmRSS: %ld", &proc.mem))
					break;
			}

			fclose(memf);

			proccesses[count] = proc;
			count++;
		}

		if (count >= proccount)
			break;
	}
	bubblesort(proccesses, proccount);
	for (int i = 0; i < 30; i++)
	{
		bprintf(PAD);
		bprintf("%-11s", proccesses[i].id);

		bprintf("%-34s", proccesses[i].name);
		bprintf("      %6.1f MB\n", proccesses[i].mem / 1000.);
	}
	free(proccesses);
	closedir(dir);
}

void print_header()
{
	bprintf(PAD CYAN "MiniHTOP\n\n\n");
}

void handleinput()
{
	char c;
}

int key_pressed()
{
	char c;
	if (read(STDIN_FILENO, &c, 1) == 1)
		return c;
	return 0;
}

int main()
{
	enableRawMode();
	while (1)
	{
		clear_screen();
		print_header();
		print_cpu();
		print_mem();
		print_proccess_count(get_process_count());
		print_uptime();
		print_proc_headers();
		print_proccess();

		if (key_pressed() == 'q')
		{
			break;
		}
		bflush();
		fflush(stdout);
		usleep(500000);
	}
	restore_terminal();
	return 0;
}
