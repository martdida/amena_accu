/*****************************************************************
 * Title    : curr_time.c
 * Author   : Martin Dida
 * Date     : 25.Jan.2017
 * Brief    : Source file for current time function implementation
 * Version  : 1.0
 * Options  : 
 ****************************************************************/
//#define _FILE_OFFSET_BITS 64
//#define SELF
/************************** Includes ****************************/
#include <time.h>
#include "../header/curr_time.h"          
#include "../header/tlpi_hdr.h"
/***************** Global Variable Definitions ******************/
// Usually put in dedicated header file with specifier "extern"



/************ Local Symbolic Constant Definitions ***************/
// When more, can be put in extra header file

#ifndef BUF_SIZE          /* Allow "gcc -D" to override definition */
#define BUF_SIZE 1000
#endif

/**************** New Local Types Definitions *******************/
// Uses "typedef" keyword to define new type



/************ Static global Variable Definitions ****************/
// Must be labeled "static"



//******** Static Local Functions Prototype Declarations ********/
// Use full prototype declarations. Must be labeled "static"



/*********************** Main Function **************************/
#ifdef SELF
int main(int argc, char *argv[])
{
  printf("Current time is: %s\n", currTime("%d_%b_%Y"));
  exit(EXIT_SUCCESS);
}
	 
#endif // SELF


/**************** Global Functions Definitions ******************/

/*  Return a string containing the current time formatted according to
    the specification in 'format' (see strftime(3) for specifiers).
    If 'format' is NULL, we use "%c" as a specifier (which gives the
    date and time as for ctime(3), but without the trailing newline).
    Returns NULL on error. */

char *
currTime(const char *format)
{
  static char buf[BUF_SIZE];
  time_t t;
  size_t s;
  struct tm *tm;

  t = time(NULL);            // Get number of seconds since Epoch start
  tm = localtime(&t);        // Break-down "t" to local time

  if (tm == NULL)
    return NULL;

  s = strftime(buf, BUF_SIZE, (format != NULL) ? format : "%c", tm);

  return ((s == 0) ? NULL : buf);
}


/***************** Local Functions Definitions ******************/
// Must be labeled "static"
