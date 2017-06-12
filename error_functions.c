/************************************************************************
 * Title     : error_functions.c
 * Date      : 08.01.2017
 * Author    : Martin Dida
 * Version   : 1.0
 * Brief     : source file for error functions
 * Options   : 
 ***********************************************************************/


/****************************** Includes *******************************/
#include <stdarg.h>
#include "../header/tlpi_hdr.h"
#include "../header/error_functions.h"
#include "ename.c.inc"                /* Defines ename and MAX_ENAME */

/******************* Global Variable Definitions ***********************/


/***************** Local Symbolic Constant Definitions *****************/
// When more can be put in extra header file. Used merely in this module
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif

/******************** New Local Types Definitions **********************/
// Uses "typedef" keyword to define new type. Used merely in this module


/****************** Static Global Variable Definitions *****************/
// Must be labeled "static". Used merely in this module


/************* Static Local Functions Prototype Declarations ***********/
// full prototype declarations


/**************************** Main Function ****************************/


/******************** Local Functions Definitions **********************/
// Must be labeled "static"

static void
terminate(Boolean useExit3)
{
  char *s;

  /* Dump core if EF_DUMPCORE environment variable is defined and
     is nonempty string otherwise call exit(3) or exit(2), 
     depending on the value of useExit3. */

  s = getenv("EF_DUMPCORE");

  if (s != NULL && *s != '\0')
    abort();
  else if (useExit3)
    exit(EXIT_FAILURE);
  else
    _exit(EXIT_FAILURE);
}


static void
outputError(Boolean useErr, int err, Boolean flushStdout,
	    const char *format, va_list ap)
{
  #define BUF_SIZE 500

  char buf[BUF_SIZE], userMsg[BUF_SIZE], errText[BUF_SIZE];

  vsnprintf(userMsg, BUF_SIZE, format, ap);

  if (useErr)
    snprintf(errText, BUF_SIZE, " [%s %s]",
	     (err > 0 && err <= MAX_ENAME) ?
	     ename[err] : "?UNKNOWN?", strerror(err));
  else
    snprintf(errText, BUF_SIZE, ":");

  snprintf(buf, BUF_SIZE, "ERRORS%s %s\n", errText, userMsg);

  if (flushStdout)
    fflush(stdout);           /* Flush any pending stdout */
  fputs(buf, stderr);
  fflush(stderr);             /* In case stderr is not line-buffered */
}


/********************* Global Functions Definitions ********************/

void
errMsg(const char *format, ...)
{
  va_list argList;
  int savedErrno;

  savedErrno = errno;            /* In case we change it here */

  va_start(argList, format);
  outputError(TRUE, errno, TRUE, format, argList);
  va_end(argList);

  errno = savedErrno;
}
  
void
errExit(const char *format, ...)
{
  va_list argList;

  va_start(argList, format);
  outputError(TRUE, errno, TRUE, format, argList);
  va_end(argList);

  terminate(TRUE);
}

void
err_exit(const char *format, ...)
{
  va_list argList;

  va_start(argList, format);
  outputError(TRUE, errno, FALSE, format, argList);
  va_end(argList);

  terminate(FALSE);
}

void
errExitEN(int errnum, const char *format, ...)
{
  va_list argList;

  va_start(argList, format);
  outputError(TRUE, errnum, TRUE, format, argList);
  va_end(argList);
  
  terminate(TRUE);
}


void
fatal(const char *format, ...)
{
  va_list argList;

  va_start(argList, format);
  outputError(FALSE, 0, TRUE, format, argList);
  va_end(argList);

  terminate(TRUE);

}

void
usageErr(const char *format, ...)
{
  va_list argList;

  fflush(stdout);         /* Flush any pending stdout */

  fprintf(stderr, "Usage: ");
  va_start(argList, format);
  vfprintf(stderr, format, argList);
  va_end(argList);

  fflush(stderr);        /* In case stderr is not line-buffered */
  exit(EXIT_FAILURE);

}

void
cmdLineErr(const char *format, ...)
{
  va_list argList;

  fflush(stdout);          /* Flush any pending stdout */

  fprintf(stderr, "Command-line usage error: ");
  va_start(argList, format);
  vfprintf(stderr, format, argList);
  va_end(argList);

  fflush(stderr);          /* In case stderr is not line-buffered */
  exit(EXIT_FAILURE);

}



