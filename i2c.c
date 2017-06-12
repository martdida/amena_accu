/*****************************************************************
 * Title    : i2c.c
 * Author   : Martin Dida
 * Date     : 23.Feb.2017
 * Brief    : Source file with I2C functionality to access I2C
 *            devices. (Amena.sk measuring system with INA129
 *                      current/power monitor with i2c interface)
 * Version  : 1.00
 * Options  : 
 ****************************************************************/
//#define _FILE_OFFSET_BITS 64

//#define SELF
//#define DEBUG
//#define PRINT
/****************************************************************/
/************************** Includes ****************************/
/****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include "../header/tlpi_hdr.h"
#include "../header/get_num.h"   /* Declares our functions for handling
				 numeric arguments (getInt(), 
				 getLong()) */
#include "../header/error_functions.h"  /* Declares our error-handling
					functions */
#include "../header/INA219.h"
#include "../header/i2c.h"

/****************************************************************/
/***************** Global Variable Definitions ******************/
/****************************************************************/
// Usually put in dedicated header file with specifier "extern"
const gid_t i2c_gid = 114;

/****************************************************************/
/************ Local Symbolic Constant Definitions ***************/
/****************************************************************/
// When more, can be put in extra header file


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




/****************************************************************/
/********* Static Local Functions Prototype Declarations ********/
/****************************************************************/
// Use full prototype declarations. Must be labeled "static"

/* @func  i2c_set_reg             - set register in slave device 
 *                                  to read/write from/to
 * @param int i2cfd               - i2c device file descriptor
 * @param const unsigned char reg - destination register in i2c slave 
 *                                  communication device
 * @return SUCCESS    - return 1 and appropriate slave device register is
 *                       set for communication
 *         ERROR      - return -1 slave device register not set 
 *                       for communication
 */
static inline int i2c_set_reg(int i2cfd, const unsigned char *reg)
{
  int ret;

  ret = write(i2cfd, reg, 1);

  return ret;
}





/****************************************************************/
/*********************** Main Function **************************/
/****************************************************************/

#ifdef SELF
int main(int argc, char **argv)
{
  int i2cfd, numWritten, numRead;
  unsigned char RDbuf[I2C_BUF_SIZE];
  unsigned char WRbuf[I2C_BUF_SIZE];
  REGS confReg = config_reg;
  REGS powReg = power_data_reg;

  if (argc < 2 || strcmp(argv[1], "--help") == 0)
    usageErr("%s <file /dev/i2c-*>\n", argv[0]);

  // Initialize i2c module to communicate with INA219 module
  i2cfd = i2c_init(argv[1], INA_SLV_ADDR);

  // Clear read/write buffers
  memset(RDbuf, 0, I2C_BUF_SIZE);
  memset(WRbuf, 0, I2C_BUF_SIZE);

  // Set configuration address 0x00 of INA219
  numWritten = i2c_write_byte_reg(i2cfd, &confReg, WRbuf);
  if (numWritten == -1)
    errExit("i2c_write_byte_reg");
  
  // Read data from configuration register
  numRead = i2c_read_data_reg(i2cfd, NULL, RDbuf, 2);
  if (numRead == -1)
    errExit("i2c_read_data_reg");

#if defined DEBUG && defined PRINT
  printf("Value of conf reg: 0x%02x%02x\n", RDbuf[0], RDbuf[1]);
#endif //  DEBUG PRINT

  // Read data from Power register
  numRead = i2c_read_data_reg(i2cfd, &powReg, RDbuf, 2);
  if (numRead == -1)
    errExit("i2c_read_data_reg");

#if defined DEBUG && defined PRINT
  printf("Value of power reg: 0x%02x%02x\n", RDbuf[0], RDbuf[1]);
#endif //  DEBUG PRINT
    


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

/* @func  i2c_init - function to initialize built-in i2c device
                     and set address of slave device to communicate with
 * @param  const char* device - i2c device file in /dev/i2c-*
 * @param  char slv_addr       - slave address of device with which
 *                              communication is established
 * @return SUCCESS - i2c device fd successfully open and communication
 *                   with slave on given address established
 *         ERROR   - either failure at opening i2c fd or communication
 *                   error in ioctl() system call, see apropriate errno
 */
int i2c_init(const char* device, char slv_addr)
{
  int fd, ret;

  fd = open(device, O_RDWR);
  if (fd == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_init-open-%s\" }\n", device);
    exit(EXIT_FAILURE);
  }

  ret = ioctl(fd, I2C_SLAVE, slv_addr);
  if (ret == -1) {
    fprintf(stderr,
	    "{ \"ERROR\":\"i2c_init-bad-slv_addr\" }\n");
    exit(EXIT_FAILURE);
  }

  return fd;
}


/* @func  i2c_write_data_word - function to write word (16bit) to 
                                destination register reg
 * @param  int i2cfd          - i2c device file descriptor
 * @param  const unsigned char reg - address of register to write to
 * @param  short word         - buffer keeping write data
 * @return SUCCESS            - number of written bytes
 *         ERROR              - -1 value, errno set appropriately
 */
int i2c_write_data_word(int i2cfd, const unsigned char *reg, short word)
{
  int err;
  char LSB, MSB, WRbuf[3];

  LSB = (char)(word & 0x00ff);
  word &= 0xff00;
  MSB = (char)(word >> 8); 

#if defined DEBUG && defined PRINT
  printf("0x%02x%02x\n", MSB, LSB);
#endif // DEBUG PRINT

  WRbuf[0] = *reg;
   WRbuf[1] = MSB;
   WRbuf[2] = LSB;
   
   err = write(i2cfd, WRbuf, 3);

     // Delay necessary to update previously written data
#ifdef INA219
  usleep(4);
#endif // INA219

   return err;
}
  

/*
 * @func  i2c_write_data_byte - function to write byte from WRbyte 
 *                              to register reg
 * @param  int i2cfd          - i2c device file descriptor
 * @param  const unsigned char reg - address of register to write to
 * @param  char byte          - buffer keeping byte for writing
 * @return SUCCESS            - number of written bytes, or zero on EOF
 *         ERROR              -   -1 value, errno set appropriately
 */
int i2c_write_data_byte(int i2cfd, const unsigned char *reg, char byte)
{
  int err;
  char WRbuf[2];
  
   WRbuf[0] = *reg;
   WRbuf[1] = byte;

   err = write(i2cfd, WRbuf, 2);
   
   // Delay necessary to update previously written data
#ifdef INA219
  usleep(4);
#endif // INA219

   return err;
}

/* @func  i2c_read_data_word - function to read word from register reg
 * @param  int i2cfd         - i2c device file descriptor
 * @param  const unsigned char reg - address of register to read from
 * @param  char *word        - buffer to store read data
 * @return SUCCESS           - number of read bytes, or zero on EOF
 *         ERROR             - -1 value, errno set appropriately
 */
int i2c_read_data_word(int i2cfd, const unsigned char *reg, char *word)
{
  int err;

  if (reg == NULL)
    err = read(i2cfd, word, 2);
  else {
    err = i2c_set_reg(i2cfd, reg);
    if (err == -1)
      return err;
    err = read(i2cfd, word, 2);
  }
    
  return err;
}


/*
 * @func  i2c_read_byte_reg - function to read byte to RDbyte 
 *                            from register reg
 * @param  int i2cfd - i2c device file descriptor
 * @param  void *reg - address of register to read from
 *                     if reg = NULL read from the register's address of
 *		       previous reading
 * @param  void *RDbyte - buffer keeping read data 
 * @return SUCCESS   -   number of read bytes, or zero on EOF
 *         ERROR     -   -1 value, errno set appropriately
 */
int i2c_read_byte_reg(int i2cfd, const unsigned char *reg, char byte)
{
  int err;

  if (reg == NULL)
    err = read(i2cfd, &byte, 1);
  else {
    i2c_set_reg(i2cfd, reg);
    err = read(i2cfd, &byte, 1);
  }

  return err;
}




  
  


  
  
  



