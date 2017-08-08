#define OPTION_RESET (EEPSIZE-1)
#define OPTION_MEMORY (EEPSIZE-2)
#define OPTION_LASTMODE (EEPSIZE-3)

#define MAX_MODES 10
#define MODES_OFFSET (EEPSIZE-10)
#define TYPES_OFFSET (EEPSIZE-20)

#include "attiny25.h"

#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#define RAMP_7135 3,3,4,5,6,8,10,12,15,19,23,28,33,40,47,55,63,73,84,95,108,122,137,153,171,190,210,232,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define RAMP_FET 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,5,8,11,14,18,22,26,30,34,39,44,49,54,59,65,71,77,84,91,98,105,113,121,129,137,146,155,164,174,184,194,205,216,255

PROGMEM const uint8_t ramp_7135[] = { RAMP_7135 };
PROGMEM const uint8_t ramp_FET[]  = { RAMP_FET };

// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t factory_settings[] = { 0,  0,  0,  0, 0, 0, 0, 0, 0, 0 };
PROGMEM const uint8_t factory_modes[]    = { 3, 11, 30, 58, 0, 0, 0, 0, 0, 0 };
PROGMEM const uint8_t factory_types[]    = { 0,  0,  0,  0,  1, 0, 0, 0, 0, 0 };

uint8_t user_modes[MAX_MODES] = { 3, 11, 30, 58, 0, 0, 0, 0, 0, 0 };
uint8_t user_types[MAX_MODES] = { 0,  0,  0,  0,  1, 0, 0, 0, 0, 0 };

inline void set_output(uint8_t pwm1, uint8_t pwm2) {
  PWM_LVL = pwm1;
  ALT_PWM_LVL = pwm2; 
}

/* uint8_t read_option(uint8_t o) { */
/*   return eeprom_read_byte((uint8_t *)(o)); */
/* } */

/* void write_option(uint8_t o, uint8_t val) { */
/*   eeprom_write_byte((uint8_t *)o, val); */
/* } */

uint8_t read_otc() {
    DIDR0 |= (1 << CAP_DIDR);
    ADMUX  = (1 << V_REF) | (1 << ADLAR) | CAP_CHANNEL;
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;

    while (ADCSRA & (1 << ADSC));
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADCH;
}

uint8_t mode_count(uint8_t *m) {
  uint8_t n = 0;
  
  while (m[n] != 0) {
    ++n;
  }

  return n;
}

void save_modes() {
  uint8_t n = 0; 
  while (n < MAX_MODES) {
    eeprom_write_byte((uint8_t *)(MODES_OFFSET-n), user_modes[n]);
    eeprom_write_byte((uint8_t *)(TYPES_OFFSET-n), user_types[n]);
    n++;
  }
}

// combine read and save?
void read_modes() {
  uint8_t n = 0;
  
  while (n < MAX_MODES) {
    user_modes[n] = eeprom_read_byte((uint8_t *)MODES_OFFSET-n);
    user_types[n] = eeprom_read_byte((uint8_t *)TYPES_OFFSET-n);
    n++;
  }
}

void array_insert(uint8_t *a, uint8_t len, uint8_t pos, uint8_t val) {
  while (pos < len)
    a[len] = a[--len];
  a[pos] = val;
}

void array_delete(uint8_t *a, uint8_t len, uint8_t pos) {
  while (pos < len)
    a[pos] = a[++pos];
  a[len] = 0; 
}

void set_level(uint8_t level) {
  PWM_LVL = pgm_read_byte(ramp_FET + level); 
  ALT_PWM_LVL = pgm_read_byte(ramp_7135 + level);
}

uint8_t next_mode(uint8_t *m, uint8_t index) {
  return ((m[index+1]) ? : m[0]);
}

int main(void) {

  // write_option(127, 3);
  // write_option(126, 1);

  eeprom_write_byte((uint8_t *)(OPTION_LASTMODE), 0);
  eeprom_write_byte((uint8_t *)(OPTION_MEMORY), 1);

  uint8_t cap_val = read_otc();
  uint8_t mode = eeprom_read_byte((uint8_t *)(OPTION_LASTMODE));
  uint8_t memory = eeprom_read_byte((uint8_t *)(OPTION_MEMORY));

  // Set PWM pin to output
  DDRB |= (1 << PWM_PIN);     // enable main channel
  DDRB |= (1 << ALT_PWM_PIN); // enable second channel
  
  TCCR0A = PHASE; // Set timer to do PWM for correct output pin and set prescaler timing
  TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

  save_modes();
  read_modes();

  if (cap_val > CAP_SHORT) {

    if (mode < 3) {
      mode++;
    } else {
      mode = 0;
    }
      
    eeprom_write_byte((uint8_t *)(OPTION_LASTMODE), mode);
  }
  
  // charge up the cap
  DDRB  |= (1 << CAP_PIN);    // Output
  PORTB |= (1 << CAP_PIN);    // High

  set_level(user_modes[(memory != 0) ? mode : 0]);

  while(1) {
    next_mode(user_modes, mode);
    //   _delay_ms(100);
  }
}
