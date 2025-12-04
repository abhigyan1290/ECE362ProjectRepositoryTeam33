#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "keypad_mapped.h"
#include "chardisp.h" // <-- make sure this declares init_chardisp_pins, cd_init, cd_display1, cd_display2
#include "outputbuilder.h"
#include "hardware/adc.h"

const bool USING_LCD = true; // Set to true if using LCD, false if using OLED, for check_wiring.
const int SEG7_DMA_CHANNEL = 5;

const int SPI_DISP_SCK = 34; // Replace with your SCK pin number for the LCD/OLED display
const int SPI_DISP_CSn = 33; // Replace with your CSn pin number for the LCD/OLED display
const int SPI_DISP_TX = 35;  // Replace with your TX pin number for the LCD/OLED display

#define LCD_COLS 16
#define EXPR_MAX 63
#define ADC_PIN 45    // pot connected to GPIO 45 (from Lab 4)
#define ADC_CHANNEL 5 // ADC channel 5 on RP2350

static uint8_t last_outputs[8];
static bool have_table = false;
static int current_row = 0;
static char last_expr[EXPR_MAX + 1];

static char expr_buf[EXPR_MAX + 1];
static int expr_len = 0;

static char lcd_line1[LCD_COLS + 1];
static char lcd_line2[LCD_COLS + 1];
static int lcd_row = 0; // 0 = first line, 1 = second line
static int lcd_col = 0; // 0..15

static void expr_clear(void)
{
    expr_len = 0;
    expr_buf[0] = '\0';
}

// ---------------- LCD helper functions ----------------

static void lcd_clear_buffers(void)
{
    memset(lcd_line1, ' ', LCD_COLS);
    memset(lcd_line2, ' ', LCD_COLS);
    lcd_line1[LCD_COLS] = '\0';
    lcd_line2[LCD_COLS] = '\0';
    lcd_row = 0;
    lcd_col = 0;
}

static void lcd_sync(void)
{
    cd_display1(lcd_line1);
    cd_display2(lcd_line2);
}

// Put a single printable character at current cursor position,
// with automatic wrap from line 1 -> line 2.
static void lcd_put_char(char c)
{
    if (c == '\n')
    {
        // ENTER just goes to the start of line 2
        lcd_row = 1;
        lcd_col = 0;
        return;
    }

    // If first line is full, wrap to second line
    if (lcd_row == 0 && lcd_col >= LCD_COLS)
    {
        lcd_row = 1;
        lcd_col = 0;
    }

    // If second line is full, you can either:
    //  - ignore extra characters, or
    //  - overwrite the last one. For now, ignore.
    if (lcd_row == 1 && lcd_col >= LCD_COLS)
    {
        return;
    }

    if (lcd_row == 0)
    {
        lcd_line1[lcd_col++] = c;
    }
    else
    {
        lcd_line2[lcd_col++] = c;
    }

    lcd_sync();
}

// Handle multi-character token (like " & ", " ^ ", etc.)
// but ignore spaces so LCD shows "&", "|", "^" compactly.
static void lcd_put_token(const char *tok)
{
    for (int i = 0; tok[i] != '\0'; ++i)
    {
        char c = tok[i];

        // Skip spaces so " & " prints as just '&' on LCD
        if (c == ' ')
            continue;

        lcd_put_char(c);
    }
}

// Handle backspace on LCD
static void lcd_backspace(void)
{
    // Nothing to delete
    if (lcd_row == 0 && lcd_col == 0)
    {
        return;
    }

    if (lcd_col > 0)
    {
        lcd_col--;
    }
    else
    {
        // At col 0 on line 2 -> go to end of line 1
        if (lcd_row == 1)
        {
            lcd_row = 0;
            lcd_col = LCD_COLS - 1;
        }
    }

    if (lcd_row == 0)
    {
        lcd_line1[lcd_col] = ' ';
    }
    else
    {
        lcd_line2[lcd_col] = ' ';
    }

    lcd_sync();
}

// For ENTER (#) – clear and start over
// static void lcd_handle_enter(void)
// {
//     lcd_clear_buffers();
//     lcd_sync();
// }

// static void lcd_show_truth_table(const char *expr, const uint8_t outputs[8])
// {
//     // Clear screen
//     lcd_clear_buffers();

//     // Line 1: expression (truncated to 16 chars)
//     lcd_row = 0;
//     lcd_col = 0;
//     for (int i = 0; i < LCD_COLS && expr[i] != '\0'; ++i)
//     {
//         lcd_put_char(expr[i]);
//     }

//     // Line 2: truth vector (8 bits as 0/1)
//     lcd_row = 1;
//     lcd_col = 0;
//     for (int i = 0; i < 8 && lcd_col < LCD_COLS; ++i)
//     {
//         lcd_put_char(outputs[i] ? '1' : '0'); // or 'T'/'F' if you prefer
//     }

//     lcd_sync();
// }

// Show a single row: expression on line 1, selected (A,B,C,F) on line 2
static void lcd_show_row(const char *expr,
                         const uint8_t outputs[8],
                         int row)
{
    lcd_clear_buffers();

    // Line 1: expression (truncated)
    lcd_row = 0;
    lcd_col = 0;
    for (int i = 0; i < LCD_COLS && expr[i] != '\0'; ++i)
    {
        lcd_put_char(expr[i]);
    }

    // Figure out A,B,C for this row index
    int A = (row >> 2) & 1;
    int B = (row >> 1) & 1;
    int C = (row >> 0) & 1;
    int F = outputs[row] ? 1 : 0;

    // Line 2: e.g. "r3:101 F=0"
    lcd_row = 1;
    lcd_col = 0;

    char line[17];
    snprintf(line, sizeof(line), "r%d:%d%d%d F=%d", row, A, B, C, F);

    for (int i = 0; line[i] != '\0' && i < LCD_COLS; ++i)
    {
        lcd_put_char(line[i]);
    }

    lcd_sync();
}

// ADC knob helpers

static void knob_adc_init(void)
{
    adc_init();
    adc_gpio_init(ADC_PIN);        // route GPIO to ADC
    adc_select_input(ADC_CHANNEL); // select channel
}

static uint16_t knob_adc_read_raw(void)
{
    // singleshot conversion
    hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);

    // wait until conversion is done
    while (!(adc_hw->cs & ADC_CS_READY_BITS))
        ;

    return (uint16_t)adc_hw->result; // 0..4095
}

// Map 0..4095 -> 0..7
static int knob_get_row_index(void)
{
    uint16_t val = knob_adc_read_raw();
    int row = (val * 8) / 4096; // 4095 maps to 7
    if (row < 0)
    {
        row = 0;
    }
    if (row > 7)
    {
        row = 7;
    }

    return row;
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    // Initialize LCD SPI display
    init_chardisp_pins();
    cd_init();
    lcd_clear_buffers();
    lcd_sync();
    expr_clear();

    // Initialize Keypad System
    q_init();
    keypad_init_pins();
    keypad_init_timer();

    // Initialize ADC knob
    knob_adc_init();
    have_table = false;
    current_row = 0;

    printf("\n========================================\n");
    printf("BOOLEAN EXPRESSION BUILDER READY\n");
    printf("Key 1=A, 2=B, 3=C\n");
    printf("Key 8=& (AND), 0=| (OR), 6=(\n");
    printf("Key 4=! (NOT), 5=^ (XOR), B=)\n");
    printf("Key #=ENTER, D=BACKSPACE\n");
    printf("========================================\n\n> ");

    while (true)
    {
        uint16_t event;

        if (key_pop(&event))
        {
            bool is_pressed = (event >> 8) & 0xFF;
            char raw_char = (char)(event & 0xFF);

            if (is_pressed)
            {
                const char *boolean_str = get_boolean_token(raw_char);

                if (boolean_str != NULL)
                {
                    // ---------- 1. SERIAL ECHO ----------
                    if (boolean_str[0] == '\b')
                    {
                        // Backspace on serial
                        printf("\b \b");
                    }
                    else if (boolean_str[0] == '\n')
                    {
                        // ENTER on serial (we'll also evaluate)
                        printf("\n");
                    }
                    else
                    {
                        // Normal token on serial
                        printf("%s", boolean_str);
                    }
                    fflush(stdout);

                    // ---------- 2. LCD ECHO + EXPR BUFFER ----------
                    if (boolean_str[0] == '\b')
                    {
                        // BACKSPACE key: update LCD and expression buffer
                        lcd_backspace();

                        if (expr_len > 0)
                        {
                            expr_len--;
                            expr_buf[expr_len] = '\0';
                        }
                    }
                    else if (boolean_str[0] == '\n')
                    {
                        // ENTER key: finish expression and compute truth table

                        // Null-terminate the expression
                        expr_buf[expr_len] = '\0';

                        uint8_t outputs[8];
                        int err = build_truth_table(expr_buf, outputs);

                        if (err == ERR_OK)
                        {
                            // Save outputs and expression for knob-based viewing
                            memcpy(last_outputs, outputs, 8);
                            strncpy(last_expr, expr_buf, EXPR_MAX);
                            last_expr[EXPR_MAX] = '\0';

                            have_table = true;

                            // Initial row based on current knob position
                            int row = knob_get_row_index();
                            current_row = row;
                            lcd_show_row(last_expr, last_outputs, current_row);

                            // Also print full truth table on serial
                            printf("Truth table (000..111): ");
                            for (int i = 0; i < 8; ++i)
                            {
                                printf("%d", outputs[i]);
                            }
                            printf("\n");
                        }
                        else
                        {
                            // Parsing failed; no valid table
                            have_table = false;

                            // Show error on LCD
                            lcd_clear_buffers();
                            lcd_row = 0;
                            lcd_col = 0;
                            const char *msg = "SYNTAX ERROR";
                            for (int i = 0; msg[i] && i < LCD_COLS; ++i)
                            {
                                lcd_put_char(msg[i]);
                            }
                            lcd_sync();

                            // Print error code on serial
                            printf("Error parsing expression (code %d)\n", err);
                        }

                        // Start new prompt on serial
                        printf("> ");
                        fflush(stdout);

                        // Reset expression for next time
                        expr_clear();
                    }
                    else
                    {
                        // Normal token (A, B, C, &, |, !, ^, (, ))

                        // If we were in table view and expr is empty,
                        // this is the start of a new expression: exit table mode.
                        if (expr_len == 0 && have_table)
                        {
                            have_table = false;
                            lcd_clear_buffers();
                            lcd_sync();
                        }

                        // 1) update LCD
                        lcd_put_token(boolean_str);

                        // 2) append to expression buffer (skip spaces just in case)
                        for (int i = 0; boolean_str[i] != '\0'; ++i)
                        {
                            char c = boolean_str[i];
                            if (c == ' ')
                                continue;

                            if (expr_len < EXPR_MAX)
                            {
                                expr_buf[expr_len++] = c;
                                expr_buf[expr_len] = '\0';
                            }
                            // else: silently drop extra chars to avoid overflow
                        }
                    }
                }
            }
        }

        // --- Knob update: if we have a valid table, use ADC to pick row ---
        if (have_table)
        {
            int row = knob_get_row_index();
            if (row != current_row)
            {
                current_row = row;
                lcd_show_row(last_expr, last_outputs, current_row);
            }
        }

        tight_loop_contents();
    }
}
