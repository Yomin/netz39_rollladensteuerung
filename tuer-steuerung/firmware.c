/*
 * Tür-Steuerung
 * Autor: Stefan Haun <tux@netz39.de>
 * 
 * Entwickelt für ATMEGA8
 * 
 * DO NOT forget to set the fuses s.th. the controller uses a 16 MHz external oscillator clock!
 */


/* define CPU frequency in MHz here if not defined in Makefile */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>


inline void setPortB(char mask) {
  PORTB |= mask;
}

inline void resetPortB(char mask) {
  PORTB &= ~mask; 
}

void set_output(const char output) {
  // store state and disable interrupts
  const uint8_t _sreg = SREG;
  cli();
  
  
  char data = output;
   
  char i;
  for (i = 0; i < 9; i++) {
    const char b = data & 0x01;
    data = data >> 1;
     
    // clear all outputs
    resetPortB((1<<PB3) | (1<<PB4));
    _delay_us(1);
     
    // set DS
    setPortB(b<<PB3);     
    _delay_us(1);
     
    // shift clock
    setPortB(1<<PB4);
    _delay_us(1);
  }

  // clear all outputs
  resetPortB((1<<PB3) | (1<<PB4));
  
  // restore state
  SREG = _sreg;  
}



void init(void) {
  /*
   * Pin-Config PortB:
   *   PB0: IN	Lockstate
   *   PB1: OUT	LED_R
   *   PB2: OUT LED_G
   *   PB3: MOSI
   *   PB4: MISO
   *   PB5: SCK
   * 
   * Pin-Config PortC:
   *   PC0: IN	Doorstate
   *   PC1: OUT	M_EN (Motor enable)
   *   PC2: OUT	M_IN1 (Motor Input direction 1)
   *   PC3: OUT	M_IN2 (Motor Input direction 2)
   *   PC4: IN	SET_CLOSE
   *   PC5: IN	SET_OPEN
   * 
   * Pin-Config PortD:
   *   PD0: RXD
   *   PD1; TXD
   *   PD2: IN	Endstop 1 (INT0)
   *   PD3: IN	Endstop 2 (INT1)
   */
  
  DDRB  = 0b1111010;
  // PullUp für Eingänge
  PORTB = 0b11111010;

   /*  disable interrupts  */
   cli();
   
   
   /*  set clock   */
  CLKPR = (1 << CLKPCE);  /*  enable clock prescaler update       */
  CLKPR = 0;              /*  set clock to maximum                */

  /*  timer init  */
  TIFR &= ~(1 << TOV0);   /*  clear timer0 overflow interrupt flag    */
  TIMSK |= (1 << TOIE0);  /*  enable timer0 overflow interrupt        */

  /*  start timer0 by setting last 3 bits in timer0 control register B
   *  to any clock source */
  //TCCR0B = (1 << CS02) | (1 << CS00);
  TCCR0B = (1 << CS00);

    
  // Global Interrupts aktivieren
  sei();  
}

int main(void)
{
  // initialisieren
  init();

  // TODO hier die main-loop

  return 0;
}


ISR (TIMER0_OVF_vect)
{
}