#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "keypad_mapped.h"
#include "chardisp.h"        // <-- make sure this declares init_chardisp_pins, cd_init, cd_display1, cd_display2

const bool USING_LCD = true; // Set to true if using LCD, false if using OLED, for check_wiring.
const int SEG7_DMA_CHANNEL = 5; 

const int SPI_DISP_SCK = 34; // Replace with your SCK pin number for the LCD/OLED display
const int SPI_DISP_CSn = 33; // Replace with your CSn pin number for the LCD/OLED display
const int SPI_DISP_TX = 35; // Replace with your TX pin number for the LCD/OLED display

#define LCD_COLS 16

static char lcd_line1[LCD_COLS + 1];
static char lcd_line2[LCD_COLS + 1];
static int lcd_row = 0;      // 0 = first line, 1 = second line
static int lcd_col = 0;      // 0..15

// ---------------- LCD helper functions ----------------

static void lcd_clear_buffers(void) {
    memset(lcd_line1, ' ', LCD_COLS);
    memset(lcd_line2, ' ', LCD_COLS);
    lcd_line1[LCD_COLS] = '\0';
    lcd_line2[LCD_COLS] = '\0';
    lcd_row = 0;
    lcd_col = 0;
}

static void lcd_sync(void) {
    cd_display1(lcd_line1);
    cd_display2(lcd_line2);
}

// Put a single printable character at current cursor position,
// with automatic wrap from line 1 -> line 2.
static void lcd_put_char(char c) {
    if (c == '\n') {
        // ENTER just goes to the start of line 2
        lcd_row = 1;
        lcd_col = 0;
        return;
    }

    // If first line is full, wrap to second line
    if (lcd_row == 0 && lcd_col >= LCD_COLS) {
        lcd_row = 1;
        lcd_col = 0;
    }

    // If second line is full, you can either:
    //  - ignore extra characters, or
    //  - overwrite the last one. For now, ignore.
    if (lcd_row == 1 && lcd_col >= LCD_COLS) {
        return;
    }

    if (lcd_row == 0) {
        lcd_line1[lcd_col++] = c;
    } else {
        lcd_line2[lcd_col++] = c;
    }

    lcd_sync();
}

// Handle multi-character token (like " & ", " ^ ", etc.)
// but ignore spaces so LCD shows "&", "|", "^" compactly.
static void lcd_put_token(const char *tok) {
    for (int i = 0; tok[i] != '\0'; ++i) {
        char c = tok[i];

        // Skip spaces so " & " prints as just '&' on LCD
        if (c == ' ') continue;

        lcd_put_char(c);
    }
}

// Handle backspace on LCD
static void lcd_backspace(void) {
    // Nothing to delete
    if (lcd_row == 0 && lcd_col == 0) {
        return;
    }

    if (lcd_col > 0) {
        lcd_col--;
    } else {
        // At col 0 on line 2 -> go to end of line 1
        if (lcd_row == 1) {
            lcd_row = 0;
            lcd_col = LCD_COLS - 1;
        }
    }

    if (lcd_row == 0) {
        lcd_line1[lcd_col] = ' ';
    } else {
        lcd_line2[lcd_col] = ' ';
    }

    lcd_sync();
}

// For ENTER (#) – clear and start over
static void lcd_handle_enter(void) {
    lcd_clear_buffers();
    lcd_sync();
}
// ---------------------- main --------------------------

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // Initialize LCD SPI display
    init_chardisp_pins();
    cd_init();
    lcd_clear_buffers();
    lcd_sync();

    // Initialize Keypad System
    q_init();
    keypad_init_pins();
    keypad_init_timer();

    printf("\n========================================\n");
    printf("BOOLEAN EXPRESSION BUILDER READY\n");
    printf("Key 1=A, 2=B, 3=C\n");
    printf("Key 8=& (AND), 0=| (OR), 6=(\n");
    printf("Key 4=! (NOT), 5=^ (XOR), B=)\n");
    printf("Key #=ENTER, D=BACKSPACE\n");
    printf("========================================\n\n> ");


    while (true) {
        uint16_t event;

        if (key_pop(&event)) {

            bool is_pressed = (event >> 8) & 0xFF;
            char raw_char = (char)(event & 0xFF);

            if (is_pressed) {
                const char* boolean_str = get_boolean_token(raw_char);

                if (boolean_str != NULL) {
                    // ---------------- SERIAL ECHO ----------------
                    if (boolean_str[0] == '\b') {
                        // Backspace on serial
                        printf("\b \b");
                    } else if (boolean_str[0] == '\n') {
                        // ENTER on serial
                        printf("\n> ");
                    } else {
                        // Normal token on serial
                        printf("%s", boolean_str);
                    }
                    fflush(stdout);

                    // --------------- LCD ECHO -------------------
                    if (boolean_str[0] == '\b') {
                        // Backspace on LCD
                        lcd_backspace();
                    } else if (boolean_str[0] == '\n') {
                        // ENTER on LCD (clear and start new)
                        lcd_handle_enter();
                    } else {
                        // Normal token (may be multi-char)
                        lcd_put_token(boolean_str);
                    }
                }
            }
        }

        tight_loop_contents();
    }
}
