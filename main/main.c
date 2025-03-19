#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define TRIG_PIN  17
#define ECHO_PIN  16

static const uint TEMPO_MAX_MS = 50;

static volatile absolute_time_t s_tInicio;
static volatile absolute_time_t s_tFim;
static volatile bool s_ecoDescido       = false;
static volatile bool s_aguardandoSubida = false;
static alarm_id_t    s_alarmeFalhaId    = 0;

static volatile bool s_falhaPend = false;

static int64_t callback_falha(alarm_id_t id, void *user_data) {
    if (s_aguardandoSubida) {
        s_falhaPend = true;      
        s_aguardandoSubida = false;
    }
    return 0; 
}

static void isr_echo(uint gpio, uint32_t eventos) {
    if (eventos & GPIO_IRQ_EDGE_RISE) {
        s_tInicio = get_absolute_time();
        if (s_aguardandoSubida) {
            cancel_alarm(s_alarmeFalhaId);
            s_aguardandoSubida = false;
        }
    }
    else if (eventos & GPIO_IRQ_EDGE_FALL) {
        s_tFim = get_absolute_time();
        s_ecoDescido = true;
    }
}

int main() {
    stdio_init_all();

    datetime_t data_inicial = {
        .year  = 2025,
        .month = 3,
        .day   = 18,
        .dotw  = 0,
        .hour  = 0,
        .min   = 0,
        .sec   = 0
    };
    rtc_init();
    rtc_set_datetime(&data_inicial);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(
        ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &isr_echo
    );

    bool medindo = false;
    printf("Use 's' para iniciar medição e 'p' para pausar.\n");

    while (true) {
        int c = getchar_timeout_us(500);
        if (c == 's') {
            medindo = true;
            printf("Medições habilitadas.\n");
        } else if (c == 'p') {
            medindo = false;
            printf("Medições pausadas.\n");
        }

        if (medindo) {
            gpio_put(TRIG_PIN, 1);
            sleep_us(10);
            gpio_put(TRIG_PIN, 0);

            s_aguardandoSubida = true;
            s_alarmeFalhaId = add_alarm_in_ms(TEMPO_MAX_MS, callback_falha, NULL, false);

            if (s_ecoDescido) {
                s_ecoDescido = false;
                int pulso_us = absolute_time_diff_us(s_tInicio, s_tFim);
                double dist_cm = (pulso_us * 0.0343) / 2.0;

                datetime_t agora;
                rtc_get_datetime(&agora);
                printf("%02d:%02d:%02d - %.2f cm\n", 
                       agora.hour, agora.min, agora.sec,
                       dist_cm);
            }
        }

        if (s_falhaPend) {
            s_falhaPend = false;
            datetime_t agora;
            rtc_get_datetime(&agora);
            printf("%02d:%02d:%02d - Falha\n",
                   agora.hour, agora.min, agora.sec);
        }

        sleep_ms(1000);
    }
    return 0;
}
