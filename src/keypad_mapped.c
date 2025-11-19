#include "keypad_mapped.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

// ---------------------------------------------------------
// 1. HARDWARE DEFINITIONS
// ---------------------------------------------------------
#define ROW0 2
#define ROW1 3
#define ROW2 4
#define ROW3 5
#define COL0 6
#define COL1 7
#define COL2 8
#define COL3 9

#define ROW_MASK ((1u << ROW0) | (1u << ROW1) | (1u << ROW2) | (1u << ROW3))
#define COL_MASK ((1u << COL0) | (1u << COL1) | (1u << COL2) | (1u << COL3))

// ---------------------------------------------------------
// 2. QUEUE IMPLEMENTATION
// ---------------------------------------------------------
#define Q_SIZE 32
typedef struct {
    uint16_t buffer[Q_SIZE];
    int head;
    int tail;
} event_queue_t;

volatile event_queue_t key_q;

void q_init() {
    key_q.head = 0;
    key_q.tail = 0;
}

// Internal function to push to queue
void key_push(uint16_t event) {
    int next = (key_q.head + 1) % Q_SIZE;
    if (next != key_q.tail) {
        key_q.buffer[key_q.head] = event;
        key_q.head = next;
    }
}

bool key_pop(uint16_t *event) {
    if (key_q.head == key_q.tail) {
        return false; // Empty
    }
    *event = key_q.buffer[key_q.tail];
    key_q.tail = (key_q.tail + 1) % Q_SIZE;
    return true;
}

// ---------------------------------------------------------
// 3. KEYPAD DRIVER & ISRs
// ---------------------------------------------------------

// Global state variables
int col = -1;
static bool state[16]; 
const char keymap[17] = "DCBA#9630852*741";

// Forward declarations
void keypad_drive_column();
void keypad_isr();

void keypad_init_pins() {
    for (uint gpio = COL0; gpio <= COL3; gpio++){
        gpio_init(gpio);
        gpio_set_dir(gpio, true);
        gpio_put(gpio, 0);
    }

    for (uint gpio = ROW0; gpio <= ROW3; gpio++){
        gpio_init(gpio);
        gpio_set_dir(gpio, false);
        gpio_pull_down(gpio);
    }
    
    for(int i = 0; i < 16; i++){
        state[i] = false;
    }
    col = -1;
}

void keypad_init_timer() {
    timer_hw->alarm[0] = 0; 
    timer_hw->alarm[1] = 0;

    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    irq_set_enabled(TIMER0_IRQ_0, true);

    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);
    irq_set_enabled(TIMER0_IRQ_1, true);

    hw_set_bits(&timer_hw->inte, (1u << 0));
    hw_set_bits(&timer_hw->inte, (1u << 1));

    uint32_t time = timer_hw->timerawl;
    timer_hw->alarm[0] = time + 1000; 
    timer_hw->alarm[1] = time + 2000;
}

void keypad_drive_column() {
    timer_hw->intr = (1u << 0); 
    col++;
    if (col > 3) col = 0;

    uint mask = (1u << (COL0 + (uint)col));
    gpio_put_masked(COL_MASK, mask);

    timer_hw->alarm[0] = timer_hw->timerawl + 2000u; 
}

uint8_t keypad_read_rows() {
    uint32_t in = sio_hw->gpio_in;
    return (uint8_t)((in >> ROW0) & 0xF); 
}

void keypad_isr() {
    timer_hw->intr = (1u << 1); 
    uint8_t rows = keypad_read_rows();

    if (col < 0 || col > 3) {
        timer_hw->alarm[1] = timer_hw->timerawl + 2000u;
        return;
    }

    for (int r = 0; r < 4; r++){
        bool button_press_now = (rows >> r) & 0x1;
        int index = col * 4 + r; 
        bool was_pressed = state[index];

        if (button_press_now && !was_pressed){
            state[index] = true;
            char ch = keymap[index];
            // High byte 1 = Press
            uint16_t event = (uint16_t)((1u << 8) | (uint8_t)ch);
            key_push(event);
        }
        else if (!button_press_now && was_pressed){
            state[index] = false;
            char ch = keymap[index];
            // High byte 0 = Release
            uint16_t event = (uint16_t)((0u << 8) | (uint8_t)ch); 
            key_push(event);
        }
    }
    timer_hw->alarm[1] = timer_hw->timerawl + 2000u; 
}

// ---------------------------------------------------------
// 4. MAPPING LOGIC
// ---------------------------------------------------------
const char* get_boolean_token(char raw_key) {
    switch(raw_key) {
        case '1': return "A";
        case '2': return "B";
        case '3': return "C";
        case '8': return " & "; 
        case '0': return " | "; 
        case '4': return "!";   
        case '5': return " ^ "; 
        case '6': return "(";   
        case 'B': return ")";   
        case '#': return "\n";  
        case 'D': return "\b";  
        default: return NULL;   
    }
}