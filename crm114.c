#define MAX_OPTIONS 10

#define RESET 0
#define MEMORY 0
#define LASTMODE 0
#define GROUP 0

#define CONFIG_DELAY 2000 // the delay upon entering configuration mode when the light is off
#define CONFIG_MODE_LEVEL 11 // we use this for confirmation blinks during configuration mode

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
PROGMEM const uint8_t factory_modes[]    = { 3, 11, 20, 54, 0, 0, 0, 0, 0, 0 };

// don't initialize to zero during startup
uint8_t fast_press_count __attribute__ ((section (".noinit")));
uint8_t config_mode __attribute__ ((section (".noinit")));
uint8_t config_sub_menu __attribute__ ((section (".noinit")));
uint8_t config_sub_arg __attribute__ ((section (".noinit")));

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

/* Function: set_level()
 *
 * Take a level, look up it's actual values and call set_output()
 *
 */
void set_level(uint8_t level) {
  set_output(pgm_read_byte(ramp_FET + level), pgm_read_byte(ramp_7135 + level));
}

/* Function: ramp()
 *
 *
 *
 */
void ramp(uint8_t level, uint8_t speed) {
  static uint8_t actual_level = 0;
  int8_t shift_amount;
  int8_t diff;

  do {
    diff = level - actual_level;
    shift_amount = (diff >> 2) | (diff!=0);
    actual_level += shift_amount;
    set_level(actual_level);
    for (uint8_t d = 0; d < speed; d++) {
      _delay_ms(8);
    }
  } while (level != actual_level);
}

/* Function: ramp_both_ways()
 *
 * Ramp up and down. Come on, think of a better name...
 *
 */
void ramp_both_ways(uint8_t top, uint8_t bottom, uint8_t count, uint8_t speed) {
  uint8_t n = count;

  while (n > 0) {
    ramp(top, speed);
    ramp(bottom, speed);
    n--;
  }
}

/* Function: off()
 *
 * Turn the LED off. Having a bunch of set_level(0); calls hanging around is ugly.
 *
 */
void off() {
  set_level(0);
}

/* Function: blink()
 *
 * Don't blink, I know just what you're thinking. 
 *
 */
void blink(uint8_t count) {
  uint8_t n = count;
  off();
  while(n >= 0) {
    set_level(30);
    _delay_ms(200);
    off();
    n--;
  }
}


/* Function: confirm_config()
 *
 * Something soothing to let the user know they've entered configuration mode.
 *
 */
void confirm_config() {
  ramp_both_ways(32, 3, 2, 2);
  off();
}

/* Function: config_change()
 *
 * Something soothing to let the user know they've changed a value.
 *
 */
void confirm_change() {
  ramp_both_ways(32, 3, 2, 1);
  off();
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

  fast_press_count = 0;
  config_mode = 0;
  config_sub_menu = 0;
  config_sub_arg = 0;

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
  uint8_t mode_index = read_option(OPT_LASTMODE);
  uint8_t modes[MAX_MODES];
  read_modes(modes, 0);

  // Set PWM pin to output
  DDRB |= (1 << PWM_PIN);     // enable main channel
  DDRB |= (1 << ALT_PWM_PIN); // enable second channel
  TCCR0A = PHASE; // Set timer to do PWM for correct output pin and set prescaler timing
  TCCR0B = 0x01; // pre-scaler for timer (1 => 1, 2 => 8, 3 => 64...)

  // if we see a value greater than CAP_SHORT, the user is trying to enter config mode
  // if we're already in config mode, relax the speed a little bit
  // otherwise let's reset the fast_press_count
  if ((config_mode && (cap_val > CAP_MEDIUM)) || (cap_val > CAP_SHORT)) {
    fast_press_count += 1;
  } else {
    config_mode = 0;
    fast_press_count = 0;
  }

  // if we see a value greater than CAP_MEDIUM, advance the mode
  // also advances if we see a fast press
  if (cap_val > CAP_MEDIUM) {
    mode_index = next_mode(modes, mode_index);
  // otherwise return to the first mode if memory is not turned on
  } else if (!memory) {
    mode_index = 0; 
  }

  // turn the light on
  if (config_mode != 1) {
    set_mode(modes, mode_index);
  } else {
    off();
    set_level(CONFIG_MODE_LEVEL);
    _delay_ms(100);
    off();
  }

  // charge up the cap
  charge_otc(); 

  while(1) {

    // enter into config mode 
    if (config_mode == 1) {
      _delay_ms(1000);
      if (fast_press_count == 2) {
	write_option(OPT_MEMORY, !read_option(OPT_MEMORY));
	confirm_change();
      } 

      // reset the fast press count
      // turn the light back on, in the first mode?
      // not sure what makes the most sense here
      //      _delay_ms(CONFIG_DELAY);
      config_mode = 0; // make sure we exit config mode
      fast_press_count = 0;
      set_mode(modes, 0);
    }

    // do something better here...
    // maybe alternate between fast and slow presses to enter config?
    if(fast_press_count > 10) {
      off();
      _delay_ms(1000);
      confirm_config();
      config_mode = 1; // enter config mode
      fast_press_count = 0; // reset fast press
    }
  }
}
