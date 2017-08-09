#define MAX_OPTIONS 10

#define RESET 0
#define MEMORY 1
#define LASTMODE 0
#define GROUP 0

#define CONFIG_DELAY 1300 // the delay upon entering configuration mode when the light is off

// these are _offsets_, not the configuration values. don't change them. 
#define OPT_OFFSET   0
#define OPT_RESET    0
#define OPT_MEMORY   1 // can we combine memory and lastmode?
#define OPT_LASTMODE 2
#define OPT_GROUP    3

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

#define RAMP_7135 0,3,4,5,6,8,10,12,15,19,23,28,33,40,47,55,63,73,84,95,108,122,137,153,171,190,210,232,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0
#define RAMP_FET 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,5,8,11,14,18,22,26,30,34,39,44,49,54,59,65,71,77,84,91,98,105,113,121,129,137,146,155,164,174,184,194,205,216,255

PROGMEM const uint8_t ramp_7135[] = { RAMP_7135 };
PROGMEM const uint8_t ramp_FET[]  = { RAMP_FET };

// Modes (gets set when the light starts up based on saved config values)
PROGMEM const uint8_t factory_settings[] = { RESET, MEMORY, LASTMODE, GROUP, 0, 0, 0, 0, 0, 0 };
PROGMEM const uint8_t factory_modes[]    = { 3, 11, 20, 30, 0, 0, 0, 0, 0, 0 };

// don't initialize to zero during startup
uint8_t fast_press_count __attribute__ ((section (".noinit")));

/* Function: set_output()
 *
 * Set the output on the PWM pins. A little more low level than set_mode so we can set levels
 * that aren't in the current mode array.
 *
 */
void set_output(uint8_t pwm1, uint8_t pwm2) {
  PWM_LVL = pwm1;
  ALT_PWM_LVL = pwm2; 
}

/* Function: read_option()
 *
 * Return the value of a given option.
 *
 */
uint8_t read_option(uint16_t opt) {
  return eeprom_read_byte((uint8_t *)opt);
}

/* Function: write_option()
 *
 * Write a value to a given option.
 *
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
 *
 */
void reset() {
  uint8_t n = 0;

  // we've got 10 possible options and 10 possible modes, so loop jam
  for(n = 0; n < 10; n++) {
    // factory options to user options space
    eeprom_write_byte((uint8_t *)(OPT_OFFSET+n), pgm_read_byte(factory_settings+n));

    // factory modes to both user groups 1 and 2
    eeprom_write_byte((uint8_t *)(MODE_GROUP1_OFFSET+n), pgm_read_byte(factory_modes+n));
    eeprom_write_byte((uint8_t *)(MODE_GROUP2_OFFSET+n), pgm_read_byte(factory_modes+n));
  }
}

/* Function: read_modes()
 *
 * Take a group number and return an array with the values for that group.
 *
 */
void read_modes(uint8_t *modes, uint8_t group) {
  
  uint16_t offset = (group == 0) ? MODE_GROUP1_OFFSET : MODE_GROUP2_OFFSET;
  eeprom_read_block(modes, (void *)offset, MAX_MODES);
}

/* Function: charge_otc()
 *
 * Charge the capacitor on CAP_PIN so we can read a value from it later
 *
 */
void charge_otc() {
  DDRB  |= (1 << CAP_PIN);    // Output
  PORTB |= (1 << CAP_PIN);    // High
}

/* Function: read_otc()
 *
 * Do some magic so we can read the value from the OTC
 *
 */
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

/* Function: set_mode()
 *
 * Take an array of modes and an index, and set our PWM pins accordingly.
 *
 */
void set_mode(uint8_t *modes, uint8_t index) {
  set_output(pgm_read_byte(ramp_FET + modes[index]),
	     pgm_read_byte(ramp_7135 + modes[index]));
  write_option(OPT_LASTMODE, index);
}

/* Function: next_mode()
 *
 * Take an array of mode levels and an index, advance the index (looping if necessary).
 * Return the new index.
 *
 */
uint8_t next_mode(uint8_t *modes, uint8_t previous_index) {
  uint8_t index = modes[previous_index+1] ? previous_index+1 : 0;
  return index;
}

int main(void) {

  // do this fast, like a bunny
  uint8_t cap_val = read_otc();
  
  // do a factory reset, restoring defaults and original mode groups
  // on first boot, we're seeing -1 here. on a factory reset, we'll set it to 1
  // so if it's not false. Yeah... 
  if (read_option(OPT_RESET) != 0) {
    reset();
  }

  // load up our options and mode values
  uint8_t memory = read_option(OPT_MEMORY);
  uint8_t group = read_option(OPT_GROUP);
  uint8_t mode_index = (memory == 1) ? read_option(OPT_LASTMODE) : 0;
  uint8_t modes[MAX_MODES];
  read_modes(modes, 0);

  // Set PWM pin to output
  DDRB |= (1 << PWM_PIN);     // enable main channel
  DDRB |= (1 << ALT_PWM_PIN); // enable second channel
  TCCR0A = PHASE; // Set timer to do PWM for correct output pin and set prescaler timing
  TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

  // advance the mode if we've seen a fast press
   if (cap_val > CAP_SHORT) {
    mode_index = next_mode(modes, mode_index);
    ++fast_press_count;
  } else {
    // otherwise, reset our count
    fast_press_count = 0; 
  }
  // turn the light on
  set_mode(modes, mode_index); 
  
  // charge up the cap
  charge_otc(); 

  while(1) {
    // do something better here...
    // maybe alternate between fast and slow presses to enter config?
    if(fast_press_count > 8) {
      
      set_output(0,0);
      _delay_ms(CONFIG_DELAY);

      
      fast_press_count = 0;
      uint8_t n = 0;

      while(n < 100) {
	mode_index = next_mode(modes, mode_index);
	set_mode(modes, mode_index); 
	_delay_ms(50);
	++n;
      }
    }
  }
}
