#ifndef KEYPAD_MAPPED_H
#define KEYPAD_MAPPED_H

#include <stdint.h>
#include <stdbool.h>

// --- Initialization Functions ---
void q_init(void);
void keypad_init_pins(void);
void keypad_init_timer(void);

// --- Data Access ---
// Pop an event from the queue. Returns true if event found.
bool key_pop(uint16_t *event);

// Translates a raw key char (e.g., '8') into a token (e.g., " & ")
const char* get_boolean_token(char raw_key);

#endif