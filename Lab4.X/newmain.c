/* 
 * File:   newmain.c
 * Author: pranlal
 * ID: 1628666
 * 
 * Digital Thermometer Controller
 * The device is a temperature controller that is connected to furnace. Turns on and off the furnace
 * depending on the external or internal temperature. The device allows the user to switch between the two sensors
 * 
 * Objectives:
 * - Detect temperature with internal sensor of ATMEGA328p
 * - Detect temperature with external LM-35 sensor
 * - User defined temperature
 * - Displays temperature on the LCD, with furnace status and user set temperature
 *
 * Created on November 23, 2021, 2:41 PM
 */

#include "defines.h"

#define F_CPU 1000000UL
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include <util/delay.h>

#include "lcd.h"

/*
 * 
 */

FILE lcd_str = FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

// All the functions initialized here
void IO_in();
void External_temp();
void Internal_temp();
void Temp_set();
void Furnace_status();
void Mode();
void Furnace_Light();

float external;
int External_integer;
float final_ext;
int External_decimal;

float internal_calibrated;
float internal_uncalibrated;
int Internal_integer;
float final_int;
int Internal_decimal;

int user_temp = 23;
int counter = 0;
int light = 0;

float compare_temp;

int main(int argc, char** argv) {

    IO_in();

    // While loop to refresh the LCD screen at 505ms (refresh rate)
    while (1) {
        External_temp();
        Internal_temp();
        Temp_set();

        lcd_init();

        Mode();
        Furnace_status();

        _delay_ms(500);
    }

    return (1);
}

void IO_in() {

    // Pull ups and buttons used as input
    DDRC &= ~(1 << DDC3);
    PORTC |= (1 << PORTC3);
    DDRC &= ~(1 << DDC4);
    PORTC |= (1 << PORTC4);
    DDRC &= ~(1 << DDC5);
    PORTC |= (1 << PORTC5);

    // Making the port an output, and turning it off
    DDRB |= (1 << DDB0);
    PORTB &= ~(1 << PORTB0);

}

void External_temp() {
    ADCSRA |= (1 << ADEN); // turns on the ADC
    ADMUX |= (1 << REFS0); // AVCC with external capacitor at AREF pin

    ADCSRA |= (1 << ADSC); // Starts the conversion
    while (!(ADCSRA & (1 << ADIF))); // wait until conversion is complete
    ADCSRA &= ~(1 << ADIF); // Sets this bit to 0

    // Formula used for conversion from voltage to temperature
    external = (ADC * 5) / (1024 * 0.01);
    // Converts the float number to integer with 1 decimal point value
    External_integer = (int) external;
    final_ext = (external - External_integer)*10;
    External_decimal = (int) final_ext;

    // Reset the registers before it shifts to the internal mode
    ADMUX = 0x00;
    ADCSRA = 0x00;
}

void Internal_temp() {
    ADCSRA |= (1 << ADEN); // turns on the ADC
    ADMUX |= (1 << REFS1) | (1 << REFS0) | (1 << MUX3); // 

    _delay_ms(5); // Delay to ensure that the voltage 
    ADCSRA |= (1 << ADSC); // Sets this bit to 0
    while (!(ADCSRA & (1 << ADIF))); // wait until conversion is complete
    ADCSRA &= ~(1 << ADIF); // Sets this bit to 0

    // Formula used for conversion from voltage to temperature (with 1.1 reference voltage)
    internal_uncalibrated = (1.1 * ADC) / (1024 * 0.01);
    internal_calibrated = ((internal_uncalibrated) - 34.6054) / (0.074); // calibration with respect to the external sensor (LM-35)
    // Converts the float number to integer with 1 decimal point value
    Internal_integer = (int) internal_calibrated;
    final_int = (internal_calibrated - Internal_integer)*10;
    Internal_decimal = (int) final_int;

    // Reset the registers before it shifts to the External mode
    ADMUX = 0x00;
    ADCSRA = 0x00;
}

void Temp_set() {
    // Detects if button is pressed and adds or subtracts the user's desired temperature
    // While if the user is holding the button it increases in increments of 1
    if (!(PINC & (1 << PINC3))) {
        // To ensure that the limit is 35 and if the user presses it again it wraps around
        if (user_temp == 35) {
            user_temp = 9;
        }
        user_temp = user_temp + 1;
        fprintf(stderr, "C \x1b\xc0      %d\xDF", user_temp);
        // Preventing constant increase in user_temp value when the button is held
        // Ensures that it increases in increments of 1
        while (!(PINC & (1 << PINC3))) {
        };
    }
    if (!(PINC & (1 << PINC4))) {

        // To ensure that the limit is 10 and if the user presses it again it wraps around
        if (user_temp == 10) {
            user_temp = 36;
        }
        user_temp = user_temp - 1;
        fprintf(stderr, "C \x1b\xc0      %d\xDF", user_temp);
        // Preventing constant decreases in user_temp value when the button is held
        // Ensures that it decreases in increments of 1
        while (!(PINC & (1 << PINC4))) {
        };
    }
}

void Furnace_status() {
    // This incorporates the dead-band, which ensures that if the furnace is on, then when it enters the dead-band
    // It will remain on. Along side that, the light is set to show the user if the furnace is on or off
    if (compare_temp < user_temp - 0.5) {
        light = 1;
    }
    Furnace_Light();
    if (compare_temp > user_temp + 0.5) {
        light = 0;
    }
    Furnace_Light();
}

void Furnace_Light() {
    // Switches the light on or off depending on the furnace condition and prints the message if the furnace is on or off
    if (light == 1) {
        PORTB |= (1 << PORTB0);
        fprintf(stderr, "   ON");
    }
    if (light == 0) {
        PORTB &= ~(1 << PORTB0);
        fprintf(stderr, "   OFF");
    }
}

void Mode() {
    // Upon button press, it switches between the two modes, and stays in that state. 
    if (!(PINC & (1 << PINC5))) {
        counter++;
    }
    if (counter % 2 == 0) {
        // External temp reading mode
        compare_temp = external;
        stderr = &lcd_str;
        fprintf(stderr, "%d.%d\xDF", Internal_integer, Internal_decimal);
        fprintf(stderr, "C \x2D\x3E %d.%d\xDF", External_integer, External_decimal);
        fprintf(stderr, "C \x1b\xc0      %d\xDF", user_temp);
        fprintf(stderr, "C");
        // To prevent constant switching between the modes when the button is pressed
        while (!(PINC & (1 << PINC5))) {
        };
    } else {
        // Internal temp reading mode
        compare_temp = internal_calibrated;
        fprintf(stderr, "%d.%d\xDF", Internal_integer, Internal_decimal);
        fprintf(stderr, "C \x3C\x2D %d.%d\xDF", External_integer, External_decimal);
        fprintf(stderr, "C \x1b\xc0      %d\xDF", user_temp);
        fprintf(stderr, "C");
        // To prevent constant switching between the modes when the button is pressed
        while (!(PINC & (1 << PINC5))) {
        };
    }
}

