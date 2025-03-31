/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h> 
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"


const int TRIGGER_PIN_1 = 13;
const int ECHO_PIN_1 = 12;
const int TRIGGER_PIN_2 = 19;
const int ECHO_PIN_2 = 18;

volatile uint64_t pulse_start_us_1 = 0;
volatile uint64_t pulse_end_us_1 = 0;
volatile bool timeout_occurred_1 = false;

void echo_pin_callback_1(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        pulse_start_us_1 = to_us_since_boot(get_absolute_time());
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        pulse_end_us_1 = to_us_since_boot(get_absolute_time());
    }
}

volatile uint64_t pulse_start_us_2 = 0;
volatile uint64_t pulse_end_us_2 = 0;
volatile bool timeout_occurred_2 = false;

void echo_pin_callback_2(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        pulse_start_us_2 = to_us_since_boot(get_absolute_time());
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        pulse_end_us_2 = to_us_since_boot(get_absolute_time());
    }
}
void send_trigger_pulse_1() {
    gpio_put(TRIGGER_PIN_1, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN_1, 0);
}
void send_trigger_pulse_2() {
    gpio_put(TRIGGER_PIN_2, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN_2, 0);
}

void process_measurement_1() {
    datetime_t current_time;
    rtc_get_datetime(&current_time);

    if (timeout_occurred_1) {
        printf("%02d:%02d:%02d - Falha na medição\n", current_time.hour, current_time.min, current_time.sec);
    } else {
        uint64_t pulse_duration = pulse_end_us_1 - pulse_start_us_1;
        int distancia_1 = (int) (pulse_duration * 0.0343f) / 2.0f;
        printf("Sensor 1 - dist: %d cm\n", distancia_1);
    }
}
int64_t timeout_alarm_callback_1(alarm_id_t id, void *user_data) {
    timeout_occurred_1 = true;
    return 0;
}
int64_t timeout_alarm_callback_2(alarm_id_t id, void *user_data) {
    timeout_occurred_2 = true;
    return 0;
}
void process_measurement_2() {
    datetime_t current_time;
    rtc_get_datetime(&current_time);

    if (timeout_occurred_2) {
        printf("%02d:%02d:%02d - Falha na medição\n", current_time.hour, current_time.min, current_time.sec);
    } else {
        uint64_t pulse_duration = pulse_end_us_2 - pulse_start_us_2;
        int distancia_2 = (int) (pulse_duration * 0.0343f) / 2.0f;
        printf("Sensor 2 - dist: %d cm\n", distancia_2);
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN_1);
    gpio_set_dir(TRIGGER_PIN_1, GPIO_OUT);
    gpio_put(TRIGGER_PIN_1, 0);

    gpio_init(ECHO_PIN_1);
    gpio_set_dir(ECHO_PIN_1, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN_1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_pin_callback_1);

    gpio_init(TRIGGER_PIN_2);
    gpio_set_dir(TRIGGER_PIN_2, GPIO_OUT);
    gpio_put(TRIGGER_PIN_2, 0);

    gpio_init(ECHO_PIN_2);
    gpio_set_dir(ECHO_PIN_2, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN_2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_pin_callback_2);

    datetime_t init_time = {
        .year  = 2025,
        .month = 3,
        .day   = 14,
        .dotw  = 6,  
        .hour  = 11,
        .min   = 50,
        .sec   = 0
    };

    rtc_init();
    rtc_set_datetime(&init_time);

    bool measurement_enabled = false;

    printf("Digite 's' para Iniciar ou 'p' para Parar as medições:\n");

    while (true) {
        int input_char = getchar_timeout_us(100000);
        if (input_char == 's' || input_char == 'S') {
            measurement_enabled = true;
            printf("Iniciando medições...\n");
        } else if (input_char == 'p' || input_char == 'P') {
            measurement_enabled = false;
            printf("Parando medições...\n");
        }

        if (measurement_enabled) {
            pulse_start_us_1 = 0;
            pulse_end_us_1 = 0;
            timeout_occurred_1 = false;
            pulse_start_us_2 = 0;
            pulse_end_us_2 = 0;
            timeout_occurred_2 = false;
            
            alarm_id_t alarm_id_1 = add_alarm_in_ms(30, timeout_alarm_callback_1, NULL, false);
            alarm_id_t alarm_id_2 = add_alarm_in_ms(30, timeout_alarm_callback_2, NULL, false);

            send_trigger_pulse_1();
            send_trigger_pulse_2();

            while (((pulse_end_us_1 == 0) && (!timeout_occurred_1) && (pulse_end_us_2 == 0) && (!timeout_occurred_2))) {
                tight_loop_contents();
            }

            if (pulse_end_us_1 != 0) {
                cancel_alarm(alarm_id_1);
            }
            if (pulse_end_us_2 != 0) {
                cancel_alarm(alarm_id_2);
            }
            
            process_measurement_1();
            process_measurement_2();
            
            sleep_ms(1000);
        }
    }
    return 0;
}
