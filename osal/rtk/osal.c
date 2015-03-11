/*
 * Simple Open EtherCAT Master Library
 *
 * File    : osal.c
 * Version : 1.3.1
 * Date    : 11-03-2015
 * Copyright (C) 2005-2015 Speciaal Machinefabriek Ketels v.o.f.
 * Copyright (C) 2005-2015 Arthur Ketels
 * Copyright (C) 2008-2009 TU/e Technische Universiteit Eindhoven
 * Copyright (C) 2012-2015 rt-labs AB , Sweden
 *
 * SOEM is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * SOEM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * As a special exception, if other files instantiate templates or use macros
 * or inline functions from this file, or you compile this file and link it
 * with other works to produce a work based on this file, this file does not
 * by itself cause the resulting work to be covered by the GNU General Public
 * License. However the source code for this file must still be made available
 * in accordance with section (3) of the GNU General Public License.
 *
 * This exception does not invalidate any other reasons why a work based on
 * this file might be covered by the GNU General Public License.
 *
 * The EtherCAT Technology, the trade name and logo “EtherCAT” are the intellectual
 * property of, and protected by Beckhoff Automation GmbH. You can use SOEM for
 * the sole purpose of creating, using and/or selling or otherwise distributing
 * an EtherCAT network master provided that an EtherCAT Master License is obtained
 * from Beckhoff Automation GmbH.
 *
 * In case you did not receive a copy of the EtherCAT Master License along with
 * SOEM write to Beckhoff Automation GmbH, Eiserstraße 5, D-33415 Verl, Germany
 * (www.beckhoff.com).
 */

#include <osal.h>
#include <kern.h>
#include <time.h>
#include <sys/time.h>
#include <config.h>

#define  timercmp(a, b, CMP)                                \
  (((a)->tv_sec == (b)->tv_sec) ?                           \
   ((a)->tv_usec CMP (b)->tv_usec) :                        \
   ((a)->tv_sec CMP (b)->tv_sec))
#define  timeradd(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;        \
    if ((result)->tv_usec >= 1000000)                       \
    {                                                       \
       ++(result)->tv_sec;                                  \
       (result)->tv_usec -= 1000000;                        \
    }                                                       \
  } while (0)
#define  timersub(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;        \
    if ((result)->tv_usec < 0) {                            \
      --(result)->tv_sec;                                   \
      (result)->tv_usec += 1000000;                         \
    }                                                       \
  } while (0)

#define USECS_PER_SEC   1000000
#define USECS_PER_TICK  (USECS_PER_SEC / CFG_TICKS_PER_SECOND)


/* Workaround for rt-labs defect 776.
 * Default implementation of udelay() didn't work correctly when tick was
 * shorter than one millisecond.
 */
void udelay (uint32_t us)
{
   tick_t ticks = (us / USECS_PER_TICK) + 1;
   task_delay (ticks);
}

int gettimeofday(struct timeval *tp, void *tzp)
{
   tick_t tick = tick_get();
   tick_t ticks_left;

   ASSERT (tp != NULL);

   tp->tv_sec = tick / CFG_TICKS_PER_SECOND;

   ticks_left = tick % CFG_TICKS_PER_SECOND;
   tp->tv_usec = ticks_left * USECS_PER_TICK;
   ASSERT (tp->tv_usec < USECS_PER_SEC);

   return 0;
}

int osal_usleep (uint32 usec)
{
   udelay(usec);
   return 0;
}

int osal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   return gettimeofday(tv, tz);
}

ec_timet osal_current_time (void)
{
   struct timeval current_time;
   ec_timet return_value;

   gettimeofday (&current_time, 0);
   return_value.sec = current_time.tv_sec;
   return_value.usec = current_time.tv_usec;
   return return_value;
}

void osal_timer_start (osal_timert * self, uint32 timeout_usec)
{
   struct timeval start_time;
   struct timeval timeout;
   struct timeval stop_time;

   gettimeofday (&start_time, 0);
   timeout.tv_sec = timeout_usec / USECS_PER_SEC;
   timeout.tv_usec = timeout_usec % USECS_PER_SEC;
   timeradd (&start_time, &timeout, &stop_time);

   self->stop_time.sec = stop_time.tv_sec;
   self->stop_time.usec = stop_time.tv_usec;
}

boolean osal_timer_is_expired (osal_timert * self)
{
   struct timeval current_time;
   struct timeval stop_time;
   int is_not_yet_expired;

   gettimeofday (&current_time, 0);
   stop_time.tv_sec = self->stop_time.sec;
   stop_time.tv_usec = self->stop_time.usec;
   is_not_yet_expired = timercmp (&current_time, &stop_time, <);

   return is_not_yet_expired == false;
}

void *osal_malloc(size_t size)
{
   return malloc(size);
}

void osal_free(void *ptr)
{
   free(ptr);
}

int osal_thread_create(void *thandle, int stacksize, void *func, void *param)
{
   thandle = task_spawn ("worker", func, 6,stacksize, param);
   if(!thandle)
   {
      return 0;
   }
   return 1;
}

int osal_thread_create_rt(void *thandle, int stacksize, void *func, void *param)
{
   thandle = task_spawn ("worker_rt", func, 15 ,stacksize, param);
   if(!thandle)
   {
      return 0;
   }
   return 1;
}
