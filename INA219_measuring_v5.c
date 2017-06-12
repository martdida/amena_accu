/*****************************************************************
 * Title    : INA219_measuring_v5.c
 * Author   : Martin Dida
 * Date     : 26.May.2017
 * Brief    : Application to measure voltage and current consumption
 *            on LED/LCD display boards (for Amena.sk) forking to 2 processes. 
 *            Parent process displaying actual voltage current and power to user
 *            Child process calculating accumulative power log.
 *            Utilizing blocking multiplexing on stdin fd 
 *            Sharing accumulative log via shared memory object
 * Version  : v1
 * Options  : </dev/i2c-*> 
 ****************************************************************/
//#define _FILE_OFFSET_BITS 64
#define SELF
#define DEBUG
//#define PRINT
#define JSON

/****************************************************************/
/************************** Includes ****************************/
/****************************************************************/
#include <fcntl.h>
#include <time.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include "../header/tlpi_hdr.h"
#include "../header/get_num.h"   /* Declares our functions for handling
				    numeric arguments (getInt(), 
				    getLong()) */
#include "../header/error_functions.h"  /* Declares our error-handling
					   functions */
#include "../header/i2c.h"
#include "../header/INA219.h"
#include "../../header/curr_time.h"

/****************************************************************/
/***************** Global Variable Definitions ******************/
/****************************************************************/
// Usually put in dedicated header file with specifier "extern"


/****************************************************************/
/************ Local Symbolic Constant Definitions ***************/
/****************************************************************/
// When more, can be put in extra header file
#define msg "{ \"INFO\":\"Enter 'accu', 'log', 'clear', 'exit'\" }\n"

// To use 4us writing/reading delay for INA219 circuit
#define INA219


#ifndef BUF_SIZE          /* Allow "gcc -D" to override definition */
#define BUF_SIZE 1024
#endif

/****************************************************************/
/**************** New Local Types Definitions *******************/
/****************************************************************/
// Uses "typedef" keyword to define new type


/****************************************************************/
/************ Static global Variable Definitions ****************/
/****************************************************************/
// Must be labeled "static"
static volatile sig_atomic_t CheckFlag = 0;
static volatile sig_atomic_t exitFlag = 0;
static char sigChldMsg[25];

/****************************************************************/
/********* Static Local Functions Prototype Declarations ********/
/****************************************************************/
// Use full prototype declarations. Must be labeled "static"
static void sigContHandler(int sig)
{
  
  write(STDOUT_FILENO, msg, strlen(msg));
  write(STDOUT_FILENO, sigChldMsg, strlen(sigChldMsg));
}

static void sigTimer(int sig)
{
  CheckFlag = 1;
}

static void sigUsr(int sig)
{
  exitFlag = 1;
}


/****************************************************************/
/*********************** Main Function **************************/
/****************************************************************/

#ifdef SELF
int main(int argc, char *argv[])
{
  // Processe's and files related variables
  int i2cfd, nfds, readyfds, status;
  fd_set readfds;
  pid_t chldPid;
  gid_t rgid, egid;      // keeping real and effective group id
  //  char *userPath, logFilePath[256];
  char logEntry[BUF_SIZE];
  double *accuShare;
  
  // Signal related variables
  struct sigaction saCont;
  struct sigaction saTmr;
  struct sigaction saUsr;
    
  // Variable handling read/write functionality of i2c device
  int numRead, numWritten;
  char RDbuf[2];
  char command[10];

  /* Variable keeping values from registers
   * calibration register, configuration register, current register
   * shunt voltage register and 2nd complement of negative value from
   * shunt voltage
   */
  short confRegVal = 0,
    calibRegVal = 0;

  measured_data_s sIna_measuring;

  // Variable keeping real voltage and current values
  double realShuntVoltVal = 0.0;
  double realBusVoltVal = 0.0;
  double realPowerVal = 0.0;
  double realCurrVal = 0.0;
  //  double realAccuPow = 0.0;
 
  // Variables related to time and timers(needed for logs) 
  //  struct timeval timeout;
  struct itimerval itimer;
  
  //  struct tm *currTime;
  //char formTime[50];
  
  
  unsigned char configuration = config_reg;
  unsigned char calibration = calib_reg;
  unsigned char current = curr_data_reg;
  unsigned char power = power_data_reg;
  unsigned char shunt = shunt_volt_reg;
  unsigned char bus = bus_volt_reg;

  /***************************************************************************/
  /************************* PART SETTING SYSTEMS CONFIG *********************/
  /***************************************************************************/

  // Initializing of some variables
  memset(&sIna_measuring, 0, sizeof(sIna_measuring));
  memset(logEntry, 0, BUF_SIZE);
  
  // Check program's entry
  if (argc < 2 || strcmp(argv[1], "--help") == 0) {
    fprintf(stderr,
	    "{ \"INFO\":\"run %s </dev/i2c-[01]>\" }\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Initializing itimer
  itimer.it_interval.tv_sec = 1;
  itimer.it_interval.tv_usec = 0;
  itimer.it_value.tv_sec = 1;
  itimer.it_value.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &itimer, NULL) == -1)
    errExit("setitimer(ITIMER_REAL)");
  

  /* SIGCONT signal handler activation */
  sigemptyset(&saCont.sa_mask);
  saCont.sa_handler = sigContHandler;
  saCont.sa_flags = 0;
  if (sigaction(SIGCONT, &saCont, NULL) == -1)
    errExit("sigaction(SIGCONT)");

  /* SIGALRM signal handler activation */
  sigemptyset(&saTmr.sa_mask);
  saTmr.sa_handler = sigTimer;
  saTmr.sa_flags = 0;
  if (sigaction(SIGALRM, &saTmr, NULL) == -1)
    errExit("sigaction(SIGALRM)");

  /* SIGUSR1 signal handler activation */
  sigemptyset(&saUsr.sa_mask);
  saUsr.sa_handler = sigUsr;
  saUsr.sa_flags = 0;
  if (sigaction(SIGUSR1, &saUsr, NULL) == -1)
    errExit("sigaction(SIGUSR1)");

          
  /* Set effective group id to real group id to prohibit security breaches
   * Since this point egid will equal real gid = martin = 1000
   */
  egid = getegid();  // Current effect gid for subsequent i2c dev file opening
  if (setegid(getgid()) == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"setegid-real-gid\" }\n");
    exit(EXIT_FAILURE);
  }
  rgid = getegid(); // real gid after i2c dev file opening, security reason

#ifdef DEBUG
  printf("Effective gid before opening file:%d\n", (int)rgid);
#endif // DEBUG
  
  // Set effective gid back to i2c group to be able to open i2c-1 file
  if (setegid(egid) == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"setegid-effective-gid\" }");
    exit(EXIT_FAILURE);
  }

  // Open i2c device with INA's slave address to communicate with INA
  i2cfd = i2c_init(argv[1], INA_SLV_ADDR);

#ifdef DEBUG
  printf("Effective gid exactly after opening file:%d\n", (int)egid);
#endif // DEBUG
  
  /**** Set effective gid back to real gid for security reasons ****/
  if (setegid(rgid) == -1)
    errExit("{ \"ERROR\":\"setegid-back-real-gid\" }");

#ifdef DEBUG
  egid = getegid();
  printf("Effective gid back in real gid: %d, security\n", (int)egid);
#endif // DEBUG


  /****************************************************************************/
  /**************************** I2C INA-219 COMMUNICATION *********************/
  /****************************************************************************/


  /************************ Registers configuration **************************/
  // Reset configuration register on each start
  confRegVal = setreg(reset, 0, 0, 0);
  numWritten = i2c_write_data_word(i2cfd, &configuration, confRegVal);
  if (numWritten == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_write_data_word(reset-config-reg)\" }\n");
    exit(EXIT_FAILURE);
  }
	    
#ifdef DEBUG
  
  // Read init data from configuration register of INA219
  memset(RDbuf, 0, I2C_BUF_SIZE);
  numRead = i2c_read_data_word(i2cfd, &configuration, RDbuf);
  if (numRead == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_read_data_word(configuration_reg)\" }\n");
    exit(EXIT_FAILURE);
  }

  strtosh(RDbuf, confRegVal);

  printf("The init value of configuration register: 0x%02hx\n", confRegVal);

#endif // DEBUG
  
  /**********************************************************************/
  /***** Set configuration register to 0x199f and re-read its value *****/
  /**********************************************************************/
	  
  // Configure confRegValto value 0x199f (0x1fff)
  // 0x199f => shuntBusCont, SADC_Bitres12def, BADC_Bitres12def, PGA_gain8
  // 0x1fff => shuntBusCont, SADC_Sample128, BADC_Sample128, PGA_gain8
  confRegVal= setreg(shuntBusCont , SADC_Sample128, BADC_Sample128 , PGA_gain8);

  // Write confRegVal value in configuration register
  numWritten = i2c_write_data_word(i2cfd, &configuration, confRegVal);
  if (numWritten == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_write_data_word(set-config-reg)\" }\n");
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG
  // Re-read, if confRegVal value set correctly in configuration register
  memset(RDbuf, 0, I2C_BUF_SIZE);
  numRead = i2c_read_data_word(i2cfd, &configuration, RDbuf);
  if (numRead == -1) {
    fprintf(stderr,
	    "read-set-conf-register\n");
    exit(EXIT_FAILURE);
  }

  strtosh(RDbuf, confRegVal)
	  
    printf("The set value of config register: 0x%02hx\n", confRegVal);

#endif // DEBUG

  /**************** Check init value of calibration register ****************/
#ifdef DEBUG
  memset(RDbuf, 0, I2C_BUF_SIZE);
  numRead = i2c_read_data_word(i2cfd, &calibration, RDbuf);
  if (numRead == -1) {
    fprintf(stderr,
	    "i2c_read_data_word-calib-reg-init\n");
    exit(EXIT_FAILURE);
  }
  
  strtosh(RDbuf, calibRegVal)

    printf("The init value of calibration register: 0x%02hx\n", calibRegVal);

#endif // DEBUG
  
  /**********************************************************************/
  /****** Set calibration register to 0x1400 and re-read its value ******/
  /**********************************************************************/
  // Write calibRegVal value in calibration register
  calibRegVal= 0x1400;
  numWritten = i2c_write_data_word(i2cfd, &calibration, calibRegVal);
  if (numWritten == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_write_data_word(set-calib-reg)\" }\n");
    exit(EXIT_FAILURE);
  }
  
#ifdef DEBUG
  // Re-read calibRegVal value set correctly in calibration register
  memset(RDbuf, 0, I2C_BUF_SIZE);
  numRead = i2c_read_data_word(i2cfd, &calibration, RDbuf);
  if (numRead == -1) {
    fprintf(stderr,
	    "i2c_read_data_word-calib-reg-set\n");
    exit(EXIT_FAILURE);
  }

  strtosh(RDbuf, calibRegVal)
    
  printf("The set value of calibration register: 0x%02hx\n", calibRegVal);

  // Map anonymous shared mapping to share accumulative value
  // This should be inherited by child process and should
  // be shared between parent and child processes
  accuShare = mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (accuShare == MAP_FAILED) {
    fprintf(stderr,
	   "{ \"ERROR\":\"mmap(2)\" errno: %s }\n",
	   strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  // Clear shared memory object content
  *accuShare = 15;
 

#endif //DEBUG
  
  /*
   * Read Current, Power, Bus & Shunt Voltage Register values
   * convert it to human readable format, write it to log file
   */

  switch(chldPid = fork()) {
  case -1:
    fprintf(stderr,
	    "{ \"ERROR\":\"select\" errno: %s }\n"
	    , strerror(errno));

    /******************************  CHILD PROCESS  *******************************/
  case 0:
      for(;;) {

      // Measure power register continuosly each 1 second
      if (CheckFlag) {

	printf("The value of accuShare in child process: %.2f\n", *accuShare);
	
	// Read value from power register
	numRead = i2c_read_data_word(i2cfd, &power, RDbuf);
	if (numRead == -1) {
	  fprintf(stderr,
		  "{ \"ERROR\":\"i2c_read_data_word(power-reg)\" }\n");
	  exit(EXIT_FAILURE);
	}

	// Make conversions 
	strtosh(RDbuf, sIna_measuring.powerRegVal)
        realPowerVal = pwrConv(sIna_measuring.powerRegVal);

	*accuShare += realPowerVal;
	
	CheckFlag = 0;
      }

      // Check if parent does require exit
      if (exitFlag)
	_exit(EXIT_SUCCESS);
    }
   
    /*************************** PARENT PROCESS *********************************
     * Parent process checks and displays current values of measured quantities *
     * If exited then sends SIGUSR1 signal to its child process. That in turn   *
     * closes its accumulative log file and exits as well.                      *
     ****************************************************************************/
  default :

     printf(msg);
    snprintf(sigChldMsg, sizeof(sigChldMsg), "[PID]:%ld\n", (long)getpid());

    for (;;) {
      /* Set timeval to zero and make ready readfds for select syscall */
      //  timeout.tv_sec = 0;
      //timeout.tv_usec = 0;
      nfds = STDIN_FILENO + 1;
      FD_ZERO(&readfds);
      FD_SET(STDIN_FILENO, &readfds);

      // Wait for command on STDIN descriptor in blocking mode
      while ((readyfds = select(nfds, &readfds, NULL, NULL, NULL)) == -1 && errno == EINTR);
      if (readyfds == -1) {
	fprintf(stderr,
		"{ \"ERROR\":\"select\" errno: %s }\n"
		, strerror(errno));
	exit(EXIT_FAILURE);
      }

      /* Check if stdin fd already in ready state
	 and read entered command */
      if (FD_ISSET(STDIN_FILENO, &readfds) == 1) {
	scanf("%s", command);

        /*********************************** LOG **********************************/
       if ( !strcmp(command, "log") ) {

	 //Read value from shunt voltage register
	 numRead = i2c_read_data_word(i2cfd, &shunt, RDbuf);
	 if (numRead == -1) {
	   fprintf(stderr,
		   "{ \"ERROR\":\"i2c_read_data_word(shunt-volt-reg)\" }\n");
	   exit(EXIT_FAILURE);
	 }

	 strtosh(RDbuf, sIna_measuring.shuntRegVal)
	 
	 // Read value from bus voltage register
	 numRead = i2c_read_data_word(i2cfd, &bus, RDbuf);
	 if (numRead == -1) {
	   fprintf(stderr,
		   "{ \"ERROR\":\"i2c_read_data_word(bus-volt-reg)\" }\n");
	   exit(EXIT_FAILURE);
	 }

	 // Convert shunt-voltage. If negative voltage convert it to positive
	 if (sign(sIna_measuring.shuntRegVal) == -1) {
	   sIna_measuring.complVal = complement(sIna_measuring.shuntRegVal);
	   realShuntVoltVal = shuntVoltConv(sIna_measuring.complVal);
	 }
	 else {
	   realShuntVoltVal = shuntVoltConv(sIna_measuring.shuntRegVal);
	 }
	 
	 // Make bus voltage conversions
	 strtosh(RDbuf, sIna_measuring.busRegVal)
	 if (sIna_measuring.busRegVal & CNVR)
	   realBusVoltVal = busVoltConv(sIna_measuring.busRegVal);
	 else{
	    printf("Bus voltage not measured this time\n");
	 }

	 // Read value from current register
	 numRead = i2c_read_data_word(i2cfd, &current, RDbuf);
	 if (numRead == -1) {
	   fprintf(stderr,
		   "{ \"ERROR\":\"i2c_read_data_word(current-reg)\" }\n");
	   exit(EXIT_FAILURE);
	 }

	 // Make current conversions
	 strtosh(RDbuf, sIna_measuring.currRegVal)
	 realCurrVal = currConv(sIna_measuring.currRegVal);

	 // Read value from power register
	 numRead = i2c_read_data_word(i2cfd, &power, RDbuf);
	 if (numRead == -1) {
	   fprintf(stderr,
		   "{ \"ERROR\":\"i2c_read_data_word(power-reg)\" }\n");
	   exit(EXIT_FAILURE);
	 }

	 // Make power conversion
	 strtosh(RDbuf, sIna_measuring.powerRegVal)
	 realPowerVal =  pwrConv(sIna_measuring.powerRegVal);
	 
#ifdef DEBUG
	 printf("The value of busRegVal: 0x%02hx\n", sIna_measuring.busRegVal);
#endif // DEBUG
     
#ifdef JSON
	 printf("{\n\"log\":{ \"timestamp\":\"%s\", \"voltage\":%.2f, \"current\":%.2f, \"power\":%.2f }\n}\n",
		currTime("%d/%m/%y %T"),
		realBusVoltVal + (realShuntVoltVal / 1000) ,
		realCurrVal,
		realPowerVal);
#else // JSON
	 printf("The actual value of current : %.2f A\n", realCurrVal);
	 printf("The actual value of shunt voltage: %.2f mV\n", realShuntVoltVal);
	 printf("The actual value of bus voltage: %.2f\n", realBusVoltVal);
	 printf("The actual value of power: %.2f\n", realPowerVal);
#endif // JSON
       }

       /****************************** ACCU ******************************/
       else if ( !strcmp(command, "accu") ) {
	 
#ifdef JSON
	 printf("{ \"timestamp\":\"%s\", \"power\":%.2f };\n",
		currTime("%d/%m/%y %T"), *accuShare);
#else // JSON
	 printf("The actual value of power: %.2f W\n", accuPowVal);
#endif //JSON
       }

       /********************************* CLEAR *******************************/
       else if ( !strcmp(command, "clear") ) {
	 *accuShare = 0;
       }
             
       /********************************* EXIT ********************************/
       else if (strcmp(command, "exit") == 0) {
	 // Send signal to child process that is caught
	 // by sigUsr signal handler
	 kill(chldPid, SIGUSR1);
	 if (waitpid(chldPid, &status, 0) == chldPid) {
	   if (WIFEXITED(status)) {
#ifdef JSON
	     printf("{ \"INFO\":\"You are exiting %s application\" }\n", argv[0]);
#else // JSON
	     printf("You are exiting INA219_v1 application");
#endif // JSON
	     break;
	   }
	 }
       }
       else {
#ifdef JSON
	 printf("{ \"WARN\":\"Unrecognized command! Valid commands are: 'voltage', 'current', 'log', 'exit'\" }\n");
#else // JSON
	 printf("Unrecognized command!\n"
		"Valid commands are: \'accu', \'log\', \'clear\', \'exit\'\n");
#endif // JSON
       }
      }
    }
  }

  if(close(i2cfd) == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"close-i2cfd\" }\n");
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
  
#endif // SELF


/****************************************************************/
/************* Static Local Functions Definitions ***************/
/****************************************************************/
// Must be labeled "static"



  


/****************************************************************/
/**************** Global Functions Definitions ******************/
/****************************************************************/





