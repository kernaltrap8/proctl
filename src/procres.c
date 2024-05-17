// procres Copyright (C) 2024 kernaltrap8
// This program is licensed under the GNU GPLv2 and comes with
// ABSOLUTELY NO WARRENTY.
// This is free software, and you are welcome to redistribute it
// under certain conditions

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION                                                                \
  "procres v1.2\nThis program is licensed under GNU GPLv3 and comes with "     \
  "ABSOLUTELY NO WARRANTY.\nThe license "                                      \
  "document can be viewed at https://www.gnu.org/licenses/gpl-3.0.en.html\n"
#define HELP                                                                   \
  "procres\n -v, --version \n    Version and license info.\n -h, "             \
  "--help\n "                                                                  \
  "   Show this help banner.\n -k, --kill\n    Kill process without "          \
  "respawning it.\n"

#define MAX_FILENAME                                                           \
  (PATH_MAX + 10) // +10 for additional characters ("/proc//cmdline")

int get_pid_by_name(const char *proc_name) {
  DIR *dir;
  struct dirent *entry;
  char filename[128];
  FILE *fp;
  char line[256];
  int pid = -1;

  dir = opendir("/proc");
  if (dir == NULL) {
    perror("Unable to read /proc.");
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    // start parsing /proc/%s/cmdline, procname is taken from argv[1] in the
    // main function.
    if (entry->d_type == DT_DIR) {
      if (sscanf(entry->d_name, "%d", &pid) == 1) {
        if (snprintf(filename, sizeof(filename), "/proc/%s/cmdline",
                     entry->d_name) >= (int)sizeof(filename)) {
          fprintf(stderr, "Filename buffer too small\n");
          closedir(dir);
          return -1;
        }

        fp = fopen(filename, "r");
        if (fp != NULL) {
          if (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, proc_name) != NULL) {
              fclose(fp);
              closedir(dir);
              return pid;
            }
          }
          fclose(fp);
        }
      }
    }
  }
  closedir(dir);
  return -1;
}

void kill_all_instances(const char *proc_name) {
  int pid;
  while ((pid = get_pid_by_name(proc_name)) !=
         -1) { // iterate through every process. fixes a bug when killing
               // browsers.
    if (kill(pid, SIGKILL) == 0) {
      printf("Killed process %d successfully.\n", pid);
    } else {
      printf("Unable to kill process %d\n", pid);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <process_name>\n", argv[0]);
    return 1;
  }

  if (argv[1][0] == '-') {
    if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
      printf("%s", VERSION);
      return 0;
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
      printf("%s", HELP);
      return 0;
    }

    if (!strcmp(argv[1], "-k") || !strcmp(argv[1], "--kill")) {
      int pid = get_pid_by_name(argv[2]);
      if (argc < 3) {
        fprintf(stderr, "Usage: %s -k <process_name>\n", argv[0]);
        return 1;
      }
      if (pid != -1) {
        printf("Killing process \"%s\"\n", argv[2]);
        kill_all_instances(argv[2]);
        return 0;
      } else {
        printf("Unable to locate process \"%s\".\n", argv[2]);
        return 1;
      }
    } else {
      printf("Invalid argument.\n%s", HELP);
      return 1;
    }
  }

  int pid = get_pid_by_name(argv[1]);
  if (pid != -1) {
    printf("Restarting process \"%s\" (PID %d)\n", argv[1], pid);
    kill_all_instances(argv[1]);
  } else {
    printf("Unable to locate process \"%s\".\n", argv[1]);
    return 1;
  }

  // redirect stdout/stderr to /dev/null
  int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null == -1) {
    perror("open");
    return 1;
  }
  dup2(dev_null, STDOUT_FILENO);
  dup2(dev_null, STDERR_FILENO);
  close(dev_null);

  pid_t child_pid = fork();

  if (child_pid == -1) {
    perror("fork");
    return 1;
  }
  if (child_pid == 0) {
    execlp(argv[1], argv[1], NULL);
    perror("execlp");
    return 1;
  }

  return 0;
}
