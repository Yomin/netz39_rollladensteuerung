#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include <wiringPi.h>
#include <wiringPiI2C.h>

#define I2C_ADDR_CONTROLLER 0x21
#define I2C_ADDR_MANUAL     0x22

///// I2C stuff /////

/**
  * This record contains the I2C file descriptors we use in this program
  */
struct I2C_descriptors {
  int controller;
  int manual;
} I2C_fd;

#define I2C_FD_CONTROLLER (I2C_fd.controller)
#define I2C_FD_MANUAL     (I2C_fd.manual)

/**
  * Initialize an I2C channel to the specified address. Exits with an error
  * message if the initialization fails,
  *
  * @param addr The target address.
  * @return File Descriptor for the I2C channel
  */
int I2C_setup_fd(const int addr) {
  const int fd = wiringPiI2CSetup(addr);
  if (!fd) {
    printf("Error %d on I2C initialization!", errno);
    exit(-1);
  }
  return fd;
}

/**
  * Initialize all I2C channels and store the file descriptors.
  */
void I2C_init(void) {
  I2C_fd.controller = I2C_setup_fd(I2C_ADDR_CONTROLLER);
  I2C_fd.manual = I2C_setup_fd(I2C_ADDR_MANUAL);
}

#define I2C_ERR_INVALIDARGUMENT -2

int I2C_command(const int fd, const char command, const char data) {
  // check parameter range
  if ((command < 0) || (command > 0x0f))
    return I2C_ERR_INVALIDARGUMENT;
  if ((data < 0) || (data > 0x0f))
    return I2C_ERR_INVALIDARGUMENT;
  
  // TODO check fd
  
    
  // build the I2C data byte
  // arguments have been checked, 
  // this cannot be negative or more than 8 bits
  const char send = (command << 4) + data; 
  
  // send command
  const char result = wiringPiI2CReadReg8(fd, send);

  // TODO check for transmission errors
  
  return result;
}

///// I3C stuff /////

void I3C_reset_manual() {
  I2C_command(I2C_FD_MANUAL, 0x4, 0x0);
}

///// Manual Controll unit /////

#define SWITCH_UP      1
#define SWITCH_NEUTRAL 3
#define SWITCH_DOWN    2

#define SWITCH_ERR             -1
#define SWITCH_ERR_OUTOFBOUNDS -2

/**
  * Read the state of the specified switch.
  * @return 
  *	Switch state according to SWITCH_XXX, SWITCH_ERR if an error
  *	occurred.
  */
char read_switch_state(const char idx) {
  // check parameter range
  if ((idx < 1) || (idx > 3))
    return SWITCH_ERR_OUTOFBOUNDS;

  // send the command    
  const char state = I2C_command(I2C_FD_MANUAL, 0x3, idx);
  
  // return result
  return state;  
}

/**
  * Beep in the provided pattern.
  * @param The beep pattern. Only the last 4 Bits are evaluated!
  */
void beep(const char pattern) {
  I2C_command(I2C_FD_MANUAL, 0x1, pattern&0xf);
}

#define LED_PATTERN_OFF  0x00
#define LED_PATTERN_SLOW 0x01
#define LED_PATTERN_FAST 0x02
#define LED_PATTERN_ON   0x03

/**
  * Set manual mode LED to the specified blink pattern.
  * @param pattern The blink pattern; one of LED_PATTERN_XXX.
  */
void set_manual_mode_led(const char pattern) {
  I2C_command(I2C_FD_MANUAL, 0x2, pattern);
}

///// Shutter Control unit /////

#define SHUTTER_ERR             -1
#define SHUTTER_ERR_OUTOFBOUNDS -2

#define SHUTTER_OFF  0
#define SHUTTER_UP   1
#define SHUTTER_DOWN 2

char set_shutter_state(const char idx, const char state) {
  // check parameter range
  if ((idx < 1) || (idx > 3))
    return SHUTTER_ERR_OUTOFBOUNDS;
  if ((state < 0) || (state > 2))
    return SHUTTER_ERR_OUTOFBOUNDS;  

  // determine the command
  char command = 0x0;
  switch (state) {
    case SHUTTER_OFF:  command = 0x1; break;
    case SHUTTER_UP:   command = 0x2; break;
    case SHUTTER_DOWN: command = 0x3; break;
    default: {
      printf("set_shutter_state: Unknown shutter state: %d.");
      printf("This cannot happen!");
      exit(-1);
    }
  }

  // send the command    
  I2C_command(I2C_FD_CONTROLLER, command, idx);

  // return OK
  return 0;
}

void stop_all_shutters() {
  I2C_command(I2C_FD_CONTROLLER, 0x0, 0x0);
}


int main(int argc, char *argv[]) {
  I2C_init();
  
  set_manual_mode_led(LED_PATTERN_OFF);
    
  int run=1;
  while(run) {
    char sw = read_switch_state(0x1);
    printf("Switch status: %d\n", sw);
    I3C_reset_manual();
    
    switch (sw) {
      case SWITCH_NEUTRAL: set_shutter_state(0x1, SHUTTER_OFF); break;
      case SWITCH_UP: set_shutter_state(0x1, SHUTTER_UP); break;
      case SWITCH_DOWN: set_shutter_state(0x1, SHUTTER_DOWN); break;
    }

    if (sleep(1)) 
      break;
  }
  
  return 0;
}