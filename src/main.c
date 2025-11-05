#include "pico/stdlib.h"
//Board specific
#define LED_R 37
#define LED_G 38
#define LED_B 39

#define LED_ON(pin)  gpio_put(pin, 0)
#define LED_OFF(pin) gpio_put(pin, 1)

int main(void) {
    stdio_init_all();

    gpio_init(LED_R); 
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_G); 
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B); 
    gpio_set_dir(LED_B, GPIO_OUT);

    LED_OFF(LED_R); 
    LED_OFF(LED_G); 
    LED_OFF(LED_B);

    while (true) {
        // Red
        LED_ON(LED_R); 
        LED_OFF(LED_G); 
        LED_OFF(LED_B);
        sleep_ms(4000);

        // Green
        LED_OFF(LED_R); 
        LED_ON(LED_G); 
        LED_OFF(LED_B);
        sleep_ms(4000);

        // Blue
        LED_OFF(LED_R); 
        LED_OFF(LED_G); 
        LED_ON(LED_B);
        sleep_ms(4000);

        // All OFF 
        LED_OFF(LED_R); 
        LED_OFF(LED_G); 
        LED_OFF(LED_B);
        sleep_ms(3000);
    }
}
