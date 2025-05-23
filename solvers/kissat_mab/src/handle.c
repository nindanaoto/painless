#include "handle.h"

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

static void (*handler) (int);
static volatile int caught_signal;
static volatile bool handler_set;

#define SIGNALS \
SIGNAL(SIGABRT) \
SIGNAL(SIGBUS) \
SIGNAL(SIGINT) \
SIGNAL(SIGSEGV) \
SIGNAL(SIGTERM)

// *INDENT-OFF*

#define SIGNAL(SIG) \
static void (*SIG ## _handler)(int);
SIGNALS
#undef SIGNAL

const char *
kissat_mab_signal_name (int sig)
{
#define SIGNAL(SIG) \
  if (sig == SIG) return #SIG;
  SIGNALS
#undef SIGNAL
  if (sig == SIGALRM)
    return "SIGALRM";
  return "SIGUNKNOWN";
}

void
kissat_mab_reset_signal_handler (void)
{
  if (!handler_set)
    return;
#define SIGNAL(SIG) \
  signal (SIG, SIG ## _handler);
  SIGNALS
#undef SIGNAL
  handler_set = false;
  handler = 0;
}

// *INDENT-ON*

static void
catch_signal (int sig)
{
  if (caught_signal)
    return;
  caught_signal = sig;
  assert (handler_set);
  assert (handler);
  handler (sig);
  kissat_mab_reset_signal_handler ();
  raise (sig);
}

void
kissat_mab_init_signal_handler (void (*h) (int sig))
{
  assert (!handler);
  handler = h;
  handler_set = true;
#define SIGNAL(SIG) \
  SIG ##_handler = signal (SIG, catch_signal);
  SIGNALS
#undef SIGNAL
}

static volatile bool caught_alarm;
static volatile bool alarm_handler_set;
static void (*SIGALRM_handler) (int);
static void (*handle_alarm) ();

static void
catch_alarm (int sig)
{
  assert (sig == SIGALRM);
  if (caught_alarm)
    return;
  if (!alarm_handler_set)
    raise (sig);
  assert (handle_alarm);
  caught_alarm = true;
  handle_alarm ();
}

void
kissat_mab_init_alarm (void (*handler) (void))
{
  assert (handler);
  assert (!caught_alarm);
  handle_alarm = handler;
  alarm_handler_set = true;
  assert (!SIGALRM_handler);
  SIGALRM_handler = signal (SIGALRM, catch_alarm);
}

void
kissat_mab_reset_alarm (void)
{
  assert (alarm_handler_set);
  assert (handle_alarm);
  alarm_handler_set = false;
  handle_alarm = 0;
  (void) signal (SIGALRM, SIGALRM_handler);
}
