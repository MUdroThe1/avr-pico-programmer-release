#include <avr/io.h>
#include <util/delay.h>

int main() {
    DDRB = _BV(PB0); // Set PB0 as output

    while (1) {
        // Toggle port B
        PORTB ^= _BV(PB0); // Toggle PB0

        // Busy wait 1  second
        _delay_ms(1000);
    }

    return 0;
}
