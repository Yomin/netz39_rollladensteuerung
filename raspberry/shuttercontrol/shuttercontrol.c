#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/un.h>

#include <i2cbridge.h>

/**
  * Socket descriptor for communicating with i2cbridge.
  */
int sock;

#define I2C_ADDR_CONTROLLER 0x21
#define I2C_ADDR_MANUAL     0x22

/**
 * Get the milliseconds since epoch.
 */
long current_millis() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  
  return tv.tv_sec*1000L + tv.tv_usec/1000L;
}

///// I2C stuff /////

/**
 * Initialize the unix socket for communication with the i2cbridge daemon.
 * Exits with an error message if the initialization fails,
 */
void i2cbridge_init()
{
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, I2CBRIDGE_PWD "/" I2CBRIDGE_UNIX, UNIX_PATH_MAX);
  
  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("Failed to open socket");
    exit(-1);
  }
  
  printf("Connecting to i2cbridge at %s\n", addr.sun_path);
  if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1) {
    perror("Failed to connect to i2cbridge daemon");
    exit(-1);
  }
}

/**
 * Send an i2c request to the i2cbridge daemon.
 *
 * @param cmd One of the I2CBRIDGE_CMD_* cmds.
 * @param addr The I2C slave address.
 * @param reg The I2C slave register address.
 * @param data The two byte data for sending/receiving.
 * @return Result of the operation as one of I2CBRIGE_ERROR_*.
 */
int i2cbridge_send(const uint8_t cmd, const uint8_t addr, const uint8_t reg, uint8_t *data)
{
  struct i2cbridge_request req;
  struct i2cbridge_response res;
  
  req.cmd = cmd;
  req.addr = addr;
  req.reg = reg;
  memcpy(&req.data, data, 2);
  
restart:
  while(send(sock, &req, sizeof(struct i2cbridge_request), 0) == -1) {
    perror("Failed to send i2c request");
    i2cbridge_init();
    sleep(1);
  }
  
  if(recv(sock, &res, sizeof(struct i2cbridge_response), 0) == -1) {
    perror("Failed to receive i2c response");
    i2cbridge_init();
    sleep(1);
    goto restart;
  }
  
  memcpy(data, &res.data, 2);
  
  return res.status;
}

#define I2C_ERR_INVALIDARGUMENT -2

int I2C_command(const char addr, const char command, const char data) {
  // check parameter range
  if ((command < 0) || (command > 0x07))
    return I2C_ERR_INVALIDARGUMENT;
  if ((data < 0) || (data > 0x0f))
    return I2C_ERR_INVALIDARGUMENT;
  
  // build the I2C data byte
  // arguments have been checked, 
  // this cannot be negative or more than 8 bits
  unsigned char send = (command << 4) + data;
  
  // calculate the parity
  char v = send;
  char c;
  for (c = 0; v; c++) 
    v &= v-1;
  c &= 1;

  // set parity bit  
  send += (c << 7);
  
  unsigned char result[2];

  // maximal number of tries
  int hops=20;

  // try for hops times until the result is not zero
  while (--hops) {
    // send command
    if(i2cbridge_send(I2CBRIDGE_CMD_READ16, addr, send, result) != I2CBRIDGE_ERROR_OK)
      continue;

    // check for transmission errors: 2nd byte is inverted 1st byte
    if (result[0] == ~result[1])
      break;
  }
  
  if (!hops)
    printf("Giving up transmission!\n");
  
  return result[0];
}

///// I3C stuff /////

void I3C_reset_manual() {
  I2C_command(I2C_ADDR_MANUAL, 0x4, 0x0);
}

///// Manual Controll unit /////

#define SWITCH_UP      1
#define SWITCH_NEUTRAL 3
#define SWITCH_DOWN    2
#define SWITCH_LOCKED  0

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
  if ((idx < 1) || (idx > 4))
    return SWITCH_ERR_OUTOFBOUNDS;

  // send the command    
  const char state = I2C_command(I2C_ADDR_MANUAL, 0x3, idx);
  
  // return result
  return state;  
}

/**
  * Beep in the provided pattern.
  * @param The beep pattern. Only the last 4 Bits are evaluated!
  */
void beep(const char pattern) {
  I2C_command(I2C_ADDR_MANUAL, 0x1, pattern&0xf);
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
  I2C_command(I2C_ADDR_MANUAL, 0x2, pattern);
}

char get_manual_mode() {
  return I2C_command(I2C_ADDR_MANUAL, 0x5, 0);
}

#define MANUAL_MODE_ON  1
#define MANUAL_MODE_OFF 2

void set_manual_mode(const char mode) {
  I2C_command(I2C_ADDR_MANUAL, 0x5, mode);
}

///// Shutter Control unit /////

#define SHUTTER_ERR             -1
#define SHUTTER_ERR_OUTOFBOUNDS -2

#define SHUTTER_OFF  0
#define SHUTTER_UP   1
#define SHUTTER_DOWN 2

/**
  * Set the shutter control state.
  * @param idx Number of the shutter, between 1 and 4
  * @param state One of SHUTTER_XXX.
  * @return 0 if everything is okay, otherwise one of SHUTTER_ERR_XXX
  */
char set_shutter_state(const char idx, const char state) {
  // check parameter range
  if ((idx < 1) || (idx > 4))
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
      printf("set_shutter_state: Unknown shutter state: %d.", state);
      printf("This cannot happen!");
      exit(-1);
    }
  }

  // send the command    
  I2C_command(I2C_ADDR_CONTROLLER, command, idx-1);

  // return OK
  return 0;
}

/**
  * Stop all the shutters!
  */
void stop_all_shutters() {
  I2C_command(I2C_ADDR_CONTROLLER, 0x0, 0x0);
}


char switch_state[4];
long switch_lastchange[4];

/**
  * Set stored switch states to NEUTRAL.
  */
void clear_stored_switch_state() {
  const long t = current_millis();
  int i;
  for (i=0; i < 4; i++) {
    switch_state[i] = SWITCH_NEUTRAL;
    switch_lastchange[i] = t;
  }
}

/**
  * Store a new switch state.
  * Return old state if there was a change.
  * @return the old state or 0 if there was no change
  */
char store_switch_state(const char idx, const char state) {
  const char old_state = switch_state[idx-1];
  
  if (old_state == state)
    return 0;

  switch_lastchange[idx-1] = current_millis();  
    
  switch_state[idx-1] = state;
  return old_state;
}

/**
  * Adjust the switch state in state storage and controller,
  * but only if there was a change.
  */
void adjust_switch_state(const char idx, const char state) {
  // time since last change
  const long delay = 2*1000;
  const long rundelay = 60*1000;
  const long lastchange = current_millis() - switch_lastchange[idx-1];

  // only if rundelay not exceeded; 
  // after a while the shutter will be unlocked no matter what
  if (lastchange < rundelay) {
  
    // ignore neutral position for locked switches
    if ((state == SWITCH_NEUTRAL) &&
        (lastchange > delay))
        return;

    // lock switch if it is hold for longer than 2 secs
    if ((state != SWITCH_NEUTRAL) &&
        (switch_state[idx-1] == state) &&
        (lastchange > delay)) {
        printf("Locking %d.\n", idx);
        beep(0x1);
        return;
    }

  }
            
  // store new state and check if a change occured
  const char st = store_switch_state(idx, state);

  if (st) {
    // if locked, just turn off
    if (lastchange > delay) {
      printf("Shutting %d off.\n", idx);
      set_shutter_state(idx, SHUTTER_OFF);
      store_switch_state(idx, SWITCH_NEUTRAL);
    } else {
      printf("Changing switch state for %d to %d.\n", idx, state);
    
      // commit the action only if the state has changed.
      switch (state) {
        case SWITCH_NEUTRAL: set_shutter_state(idx, SHUTTER_OFF); break;
        case SWITCH_UP: set_shutter_state(idx, SHUTTER_UP); break;
        case SWITCH_DOWN: set_shutter_state(idx, SHUTTER_DOWN); break;
      }
    } // if-else  
  }
}


int main(int argc, char *argv[]) {
  i2cbridge_init();
  stop_all_shutters();
  clear_stored_switch_state();
  beep(0x05);
  set_manual_mode_led(LED_PATTERN_FAST);
  sleep(1);
  set_manual_mode_led(LED_PATTERN_OFF);
  
  char run=1;
  int i=0;
  while(run) {
    printf("****** %u\n", i++);

    const char manual = get_manual_mode();
    printf("Manual mode: %s\n", (manual==1)?"on":"off");
    if (manual == MANUAL_MODE_ON)
      set_manual_mode_led(LED_PATTERN_ON);
    else
      set_manual_mode_led(LED_PATTERN_OFF);
    
    int idx;
    for (idx=1; idx<5; idx++) {
      const char sw = read_switch_state(idx);
      printf("Switch %d status: %d\n", idx, sw);

      adjust_switch_state(idx, sw);      
    }

    I3C_reset_manual();
    if (sleep(1)) 
      break;
  }

  stop_all_shutters();
    
  return 0;
}
