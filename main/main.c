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

volatile absolute_time_t t_inicio;
volatile absolute_time_t t_fim;

volatile bool eco_descido = false;
static alarm_id_t alarme_falha_id;  
volatile bool aguardando_subida = false;

void imprimir_hora_falha(void) {
    datetime_t agora;
    rtc_get_datetime(&agora);
    printf("%02d:%02d:%02d - Falha\n", agora.hour, agora.min, agora.sec);
}

int64_t callback_falha(alarm_id_t id, void *user_data) {
    if (aguardando_subida) {
        imprimir_hora_falha();
        aguardando_subida = false;
    }
    return 0;
}

void isr_echo(uint gpio, uint32_t eventos) {
    if (eventos & GPIO_IRQ_EDGE_RISE) {
        t_inicio = get_absolute_time();
        if (aguardando_subida) {
            cancel_alarm(alarme_falha_id);
            aguardando_subida = false;
        }
    } else if (eventos & GPIO_IRQ_EDGE_FALL) {
        t_fim = get_absolute_time();
        eco_descido = true;
    }
}

void exibir_distancia(void) {
    eco_descido = false;
    int pulso_us = absolute_time_diff_us(t_inicio, t_fim);
    double dist_cm = (pulso_us * 0.0343) / 2.0;

    datetime_t agora;
    rtc_get_datetime(&agora);
    printf("%02d:%02d:%02d - %.2f cm\n", agora.hour, agora.min, agora.sec, dist_cm);
}

int main() {
    stdio_init_all();

    datetime_t data_inicial = {
        .year  = 2025,
        .month = 3,
        .day   = 19,
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
        isr_echo
    );

    bool fazendo_leitura = false;
    printf("Use 's' para iniciar e 'p' para parar.\n");

    while (true) {
        int cmd = getchar_timeout_us(500);
        if (cmd == 's') {
            fazendo_leitura = true;
            printf("Start\n");
        } else if (cmd == 'p') {
            fazendo_leitura = false;
            printf("Stop\n");
        }

        if (fazendo_leitura) {
            gpio_put(TRIG_PIN, 1);
            sleep_us(10);
            gpio_put(TRIG_PIN, 0);

            aguardando_subida = true;
            alarme_falha_id = add_alarm_in_ms(TEMPO_MAX_MS, callback_falha, NULL, false);

            if (eco_descido) {
                exibir_distancia();
            }
        }

        sleep_ms(1000);
    }
    return 0;
}
