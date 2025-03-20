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

static volatile bool s_subidaOk     = false;
static volatile bool s_descidaOk    = false; 
static volatile bool s_falhaPend    = false; 
static volatile absolute_time_t s_tInicio;
static volatile absolute_time_t s_tFim;

static int64_t callback_falha(alarm_id_t id, void *user_data) {
    if (!s_subidaOk) {
        s_falhaPend = true;
    }
    return 0; 
}

static void isr_echo(uint gpio, uint32_t eventos) {
    if (eventos & GPIO_IRQ_EDGE_RISE) {
        s_tInicio  = get_absolute_time();
        s_subidaOk = true;
    }
    else if (eventos & GPIO_IRQ_EDGE_FALL) {
        s_tFim     = get_absolute_time();
        s_descidaOk = true;
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
    printf("Use 's' para iniciar, 'p' para parar.\n");

    while (true) {
        int c = getchar_timeout_us(500);
        if (c == 's') {
            medindo = true;
            printf("Start.\n");
        }
        else if (c == 'p') {
            medindo = false;
            printf("Stop.\n");
        }

        if (medindo) {
            sleep_ms(500);

            s_subidaOk  = false;
            s_descidaOk = false;
            s_falhaPend = false;

            gpio_put(TRIG_PIN, 1);
            sleep_us(10);
            gpio_put(TRIG_PIN, 0);

            add_alarm_in_ms(TEMPO_MAX_MS, callback_falha, NULL, false);
            //sleep_ms(1000);

            while(!s_descidaOk && !s_falhaPend) {
                sleep_ms(1);
            }

            if (s_falhaPend) {
                datetime_t agora;
                rtc_get_datetime(&agora);
                printf("%02d:%02d:%02d - Falha\n",
                       agora.hour, agora.min, agora.sec);
            }
            else if (s_descidaOk) {
                int pulso_us   = absolute_time_diff_us(s_tInicio, s_tFim);
                double dist_cm = (pulso_us * 0.0343) / 2.0;

                datetime_t agora;
                rtc_get_datetime(&agora);
                printf("%02d:%02d:%02d - %.2f cm\n",
                       agora.hour, agora.min, agora.sec,
                       dist_cm);
            }
            else {
                printf("Sem borda de descida confirmada.\n");
            }
        }
        else {
            sleep_ms(500);
        }
    }
    return 0;
}
