#include <stdio.h>
#include "pico/stdlib.h"
#include "keypad_mapped.h"


int main() {
    stdio_init_all();
    
    // Initialize Keypad System
    q_init();
    keypad_init_pins();
    keypad_init_timer();

    printf("\n========================================\n");
    printf("BOOLEAN EXPRESSION BUILDER READY\n");
    printf("Key 1=A, 2=B, 3=C\n");
    printf("Key 8=AND, 0=OR, 4=NOT, 5=XOR\n");
    printf("Key #=ENTER, D=BACKSPACE\n");
    printf("========================================\n\n> ");

    while (true) {
        uint16_t event;
        
        // Check if we have data in the queue
        if (key_pop(&event)) {
            
            // Decode the event
            // Upper byte determines press (1) or release (0)
            bool is_pressed = (event >> 8) & 0xFF;
            char raw_char = (char)(event & 0xFF);

            // We only act on PRESS, ignore RELEASE
            if (is_pressed) {
                const char* boolean_str = get_boolean_token(raw_char);
                
                if (boolean_str != NULL) {
                    // Handle Backspace logic visually
                    if (boolean_str[0] == '\b') {
                        printf("\b \b"); // Move back, print space, move back
                    } 
                    // Handle Enter logic
                    else if (boolean_str[0] == '\n') {
                        printf("\n> "); 
                    }
                    // Handle standard tokens
                    else {
                        printf("%s", boolean_str);
                    }
                    
                    // Force print immediately
                    fflush(stdout);
                }
            }
        }
        
        tight_loop_contents();
    }
}