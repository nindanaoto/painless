#include "internal.h"

void
kissat_inc_init_averages (kissat * solver, averages * averages)
{
  if (averages->initialized)
    return;
#define INIT_EMA(EMA,WINDOW) \
  kissat_inc_init_smooth (solver, &averages->EMA, WINDOW, #EMA)
  INIT_EMA (level, GET_OPTION (emaslow));
  INIT_EMA (size, GET_OPTION (emaslow));
  INIT_EMA (fast_glue, GET_OPTION (emafast));
  INIT_EMA (slow_glue, GET_OPTION (emaslow));
  INIT_EMA (trail, GET_OPTION (emaslow));
  averages->initialized = true;
}
