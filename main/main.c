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

// Variáveis de “escopo de arquivo” (static), para uso pelas ISRs
static volatile bool s_subidaOk     = false; // se borda de subida do Echo ocorreu
static volatile bool s_descidaOk    = false; // se borda de descida do Echo ocorreu
static volatile bool s_falhaPend    = false; // flag: falha detectada (para imprimir fora da ISR)
static volatile absolute_time_t s_tInicio;
static volatile absolute_time_t s_tFim;

// Callback do alarme para detectar falha se a borda de subida não chegou a tempo
static int64_t callback_falha(alarm_id_t id, void *user_data) {
    // Se ainda não houve borda de subida, marcamos falha
    if (!s_subidaOk) {
        s_falhaPend = true;
    }
    return 0; // Não repete
}

// Interrupção do pino Echo
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
    printf("Use 's' p/ iniciar, 'p' p/ parar.\n");

    while (true) {
        int c = getchar_timeout_us(500);
        if (c == 's') {
            medindo = true;
            printf("Medições habilitadas.\n");
        }
        else if (c == 'p') {
            medindo = false;
            printf("Medições pausadas.\n");
        }

        if (medindo) {
            s_subidaOk  = false;
            s_descidaOk = false;
            s_falhaPend = false;

            gpio_put(TRIG_PIN, 1);
            sleep_us(10);
            gpio_put(TRIG_PIN, 0);

            add_alarm_in_ms(TEMPO_MAX_MS, callback_falha, NULL, false);

            // Esperamos ~1s entre cada tentativa
            // No meio tempo, a ISR pode mudar s_descidaOk e o alarme pode marcar s_falhaPend
            sleep_ms(1000);

            // Fora da ISR, verificamos se houve falha
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
                // Caso não tenha havido descida mas também não marcou falha, 
                // é sinal de que o Echo pode ter chegado tarde, etc.
                // Ajuste se quiser tratar esse caso separadamente.
                printf("Sem borda de descida confirmada.\n");
            }
        }
        else {
            sleep_ms(500);
        }
    }
    return 0;
}
