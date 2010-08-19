/* -*- mode: c -*-
 *
 * Nagios check_memcached_connections plugin
 *
 * License: New BSD License
 * Copyright (c) HIROSE, Masaaki. All rights reserved.
 */

const char *progname = "check_memcached_connections";
const char *revision = "$Revision: 0.1 $";
#define MY_COPYRIGHT "Copyright (c) HIROSE, Masaaki. All rights reserved.\n\
\t<%s>\n\n"
const char *email = "hirose31 _at_ gmail.com";

#include "common.h"
#include "utils.h"
#include "utils_base.h"
#include "netutils.h"

#include <libmemcached/memcached.h>

#define MEMCACHED_PORT 11211

#define EXIT_OK       0
#define EXIT_WARNING  1
#define EXIT_CRITICAL 2
#define EXIT_UNKNOWN  3


char      *mc_host   = NULL;
in_port_t  mc_port   = MEMCACHED_PORT;

int  process_arguments(int, char **);
int  validate_arguments(void);
void print_help(void);
void print_usage(void);

int verbose = 0;
thresholds *my_thresholds = NULL;

#if DO_TRACE
#define TRACE(fmt, ...)                         \
  {                                             \
    struct tm tm;                               \
    struct timeval tv;                          \
    gettimeofday(&tv, NULL);                    \
    localtime_r(&(tv.tv_sec), &tm);                            \
    printf("%02d:%02d:%02d.%06lu %.12s.%4d: (trace ) "fmt"\n", \
           tm.tm_hour,tm.tm_min,tm.tm_sec,                     \
           tv.tv_usec,                                         \
           __FILE__, __LINE__, __VA_ARGS__);                   \
  }
#else
#define TRACE(fmt, ...)
#endif

int main(int argc, char ** argv)
{
  struct memcached_st *mc;
  struct memcached_stat_st *stats;
  memcached_return_t rc;
  long             cur_conn = -1;
  int              status;
  char*            status_msg;
  struct timeval   tv;
  long             microsec;
  double           elapsed_time;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Parse extra opts if any */
  argv = np_extra_opts (&argc, argv, progname);

  if (process_arguments (argc, argv) == ERROR)
    usage4 (_("Could not parse arguments"));

  TRACE("%s",">>main");

  // initialize
  gettimeofday(&tv, NULL);
  mc = memcached_create(NULL);
  if (mc == NULL) {
    printf("MEMCACHED %s: failed to memcached_create\n", _("CRITICAL"));
    exit(EXIT_CRITICAL);
  }
  TRACE("[server]%s:%s", mc_host, mc_port);
  rc = memcached_server_add(mc, mc_host, (in_port_t)mc_port);
  TRACE("[mc_server_add rv]%d", rc);
  if (rc != MEMCACHED_SUCCESS) {
    printf("MEMCACHED %s: failed to memcached_server_add (%d)\n", _("CRITICAL"), rc);
    exit(EXIT_CRITICAL);
  }

  memcached_behavior_set(mc, MEMCACHED_BEHAVIOR_TCP_NODELAY, 1);

  stats = memcached_stat(mc, NULL, &rc);
  if (! stats) {
    printf("MEMCACHED %s: failed to memcached_stats\n", _("CRITICAL"));
    exit(EXIT_CRITICAL);
  }

  cur_conn = stats->curr_connections;

  memcached_stat_free(mc, stats);
  memcached_free(mc);

  status = get_status(cur_conn, my_thresholds);

  status_msg = _("CRITICAL");
  if (status == STATE_OK) {
      status_msg = _("OK");
  } else if (status == STATE_WARNING) {
      status_msg = _("WARNING");
  } else if (status == STATE_CRITICAL) {
      status_msg = _("CRITICAL");
  }

  microsec = deltime(tv);
  elapsed_time = (double)microsec / 1.0e6;
  printf("MEMCACHED %s: conn %ld, %.3f seconds\n", status_msg, cur_conn, elapsed_time);

  return status;
}

/* process command-line arguments */
int
process_arguments (int argc, char **argv)
{
  int c;
  char *warning  = NULL;
  char *critical = NULL;

  int option = 0;
  static struct option longopts[] = {
    {"hostname" ,required_argument, 0, 'H'},
    {"port"     ,required_argument, 0, 'P'},
    {"verbose"  ,no_argument,       0, 'v'},
    {"version"  ,no_argument,       0, 'V'},
    {"help"     ,no_argument,       0, 'h'},
    {"warning"  ,required_argument, 0, 'w'},
    {"critical" ,required_argument, 0, 'c'},
    {0, 0, 0, 0}
  };

  if (argc < 1)
    return ERROR;

  while (1) {
    c = getopt_long (argc, argv, "H:P:vVhw:c:", longopts, &option);

    if (c == -1 || c == EOF)
      break;

    switch (c) {
    case 'H':
      if (is_host(optarg)) {
        mc_host = optarg;
      }
      else {
        usage2(_("Invalid hostname/address"), optarg);
      }
      break;
    case 'P':
      mc_port = (in_port_t)atoi(optarg);
      break;
    case 'v':
      verbose++;
      break;
    case 'V':
      print_revision(progname, revision);
      exit(STATE_OK);
    case 'h':
      print_help();
      exit (STATE_OK);
    case 'w':
      warning  = optarg;
      break;
    case 'c':
      critical = optarg;
      break;
    case '?':
      usage5();
    }
  }

  c = optind;

  set_thresholds(&my_thresholds, warning, critical);

  return validate_arguments();
}


int validate_arguments(void)
{
  if (mc_host == NULL)
    usage("Must specify memcached server");

  return OK;
}


void print_help(void)
{
  char *mcport;
  asprintf(&mcport, "%d", MEMCACHED_PORT);

  print_revision (progname, revision);

  printf (_(MY_COPYRIGHT), email);

  printf ("%s\n", _("This program checks results of request sequentially: SET, GET, DELETE, GET"));

  printf ("\n\n");

  print_usage ();

  printf (_(UT_HELP_VRSN));
  printf ("%s",_(UT_EXTRA_OPTS));
  printf (_(UT_WARN_CRIT_RANGE));
  printf (_(UT_HOST_PORT), 'P', mcport);

#ifdef NP_EXTRA_OPTS
  printf ("\n");
  printf ("%s\n", _("Notes:"));
  printf (_(UT_EXTRA_OPTS_NOTES));
#endif

  /* printf (_(UT_SUPPORT)); */
  puts("\nRepository:\n  http://github.com/hirose31/nagios-check_memcached_connections/tree/master\n\n");
}


void print_usage(void)
{
  printf (_("Usage:"));
  printf (" %s -H host [-P port] [-E expire] [-w warn] [-c crit]\n", progname);
}
