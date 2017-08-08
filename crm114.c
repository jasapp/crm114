#define MAX_OPTIONS 10
#define OPT_OFFSET   0
#define OPT_RESET    0
#define OPT_MEMORY   1 // can we combine memory and lastmode?
#define OPT_LASTMODE 2

#define MAX_MODES 10
#define MODE_GROUP1_OFFSET 10
#define MODE_GROUP2_OFFSET 20

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
PROGMEM const uint8_t factory_settings[] = { 0,  1,  0,  0, 0, 0, 0, 0, 0, 0 };
PROGMEM const uint8_t factory_modes[]    = { 3, 11, 30, 58, 0, 0, 0, 0, 0, 0 };

// uint8_t user_mode[MAX_MODES] = { 3, 11, 30, 58, 0, 0, 0, 0, 0, 0 };

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

/* Function: read_option()
 *
 * Return the value of a given option.
 */
uint8_t read_option(uint16_t opt) {
  return eeprom_read_byte((uint8_t *)opt);
}

/* Function: write_option()
 *
 * Write a value to a given option.
 */
void write_option(uint16_t opt, uint8_t val) {
  eeprom_write_byte((uint8_t *)opt, val);
}

/* Function: reset() 
 *
 * Restore all options and mode groups to their original values
 * Both user mode groups are restored to the same values. If a user
 * accidentally manages to switch between a mode groups, when both
 * mode groups are default, they will not be surprised. Right?
 */
void reset() {
  uint8_t n = 0;

  // use eeprom_read_block here instead?
  // we've got 10 possible options and 10 possible modes, so loop jam
  for(n = 0; n < 10; n++) {

    // factory options to user options space
    eeprom_write_byte((uint8_t *)(OPT_OFFSET+n),
		      eeprom_read_byte((uint8_t *)(factory_settings+n)));

    // factory modes to both user groups 1 and 2
    eeprom_write_byte((uint8_t *)(MODE_GROUP1_OFFSET), 
		      eeprom_read_byte((uint8_t *)(factory_modes+n)));
    eeprom_write_byte((uint8_t *)(MODE_GROUP2_OFFSET),
		      eeprom_read_byte((uint8_t *)(factory_modes+n)));
  }
}

uint8_t read_otc() {
    DIDR0 |= (1 << CAP_DIDR);
    ADMUX  = (1 << V_REF) | (1 << ADLAR) | CAP_CHANNEL;
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;

    while (ADCSRA & (1 << ADSC));
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADCH;
}

void save_modes(uint8_t *modes, uint8_t group) {
  
}

// combine read and save?
void read_modes(uint8_t *modes, uint8_t group) {
  uint8_t n = 0;
  
  while (n < MAX_MODES) {
    modes[n] = eeprom_read_byte((uint8_t *)(MODE_GROUP1_OFFSET+n));
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

void set_mode(uint8_t *modes, uint8_t level) {
  PWM_LVL = pgm_read_byte(ramp_FET + level); 
  ALT_PWM_LVL = pgm_read_byte(ramp_7135 + level);
}

uint8_t next_mode(uint8_t *m, uint8_t index) {
  return ((m[index+1]) ? : m[0]);
}

int main(void) {
  // do this fast, like a bunny
  uint8_t cap_val = read_otc();

  // always do a reset at the moment
  write_option(OPT_RESET, 1);
  
  // do a factory reset, restoring defaults and original mode groups
  if (read_option(OPT_RESET) == 1) {
    reset();
  }

  //   uint8_t modes[MAX_MODES];
  uint8_t memory = read_option(OPT_MEMORY);
  uint8_t mode = (memory == 1) ? read_option(OPT_MEMORY) : 0;

  // Set PWM pin to output
  DDRB |= (1 << PWM_PIN);     // enable main channel
  DDRB |= (1 << ALT_PWM_PIN); // enable second channel
  
  TCCR0A = PHASE; // Set timer to do PWM for correct output pin and set prescaler timing
  TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

  if (cap_val > CAP_SHORT) {

    if (mode < 3) {
      mode++;
    } else {
      mode = 0;
    }
      
    eeprom_write_byte((uint8_t *)(OPT_LASTMODE), mode);
  }
  
  // charge up the cap
  DDRB  |= (1 << CAP_PIN);    // Output
  PORTB |= (1 << CAP_PIN);    // High

  //   set_level(user_modes[(memory != 0) ? mode : 0]);

  while(1) {
    //    next_mode(user_modes, mode);
    //   _delay_ms(100);
  }
}
