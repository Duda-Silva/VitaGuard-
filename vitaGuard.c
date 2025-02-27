#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h" //biblioteca para configuração de pinos
#include "hardware/adc.h" //biblioteca para configuração do ADC
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"
#include "pico/bootrom.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

//Configuração de pinos
#define LED_RED_PIN 13
#define LED_BLUE_PIN 12
#define LED_GREEN_PIN 11
#define BUTTON_B_PIN 6 //botão emergência
#define JOYSTICK_X_PIN 27
#define JOYSTICK_Y_PIN 26
#define BUZZER_PIN 21 //ALARME
#define MIC_PIN 28 //MICROFONE
#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define WS2812_PIN 7 //MATRIZ DE LEDS
#define I2C_PORT i2c1
#define endereco 0x3C

// declaração de variaveis globais
ssd1306_t ssd;
volatile bool emergencia_acionada = false;
const uint limiar_1 = 2080;     // Limiar para o LED verde

bool cor = true;
const uint amostras_por_segundo = 8000; // Frequência de amostragem (8 kHz)


// Protótipos das funções
uint init_pins(); //inicializa os pinos
void init_display(); //inicializa o display
void detect_fall(); //detecta quedas
void check_emergency_a_button(uint gpio, uint32_t events); //verifica o botão de emergência
int64_t turn_off_blue_led(alarm_id_t id, void *user_data); //desliga o LED azul
void monitor_activity(); //monitora a atividade
void monitor_noise(); //monitora o ruído




//Função principal
int main()
{
    stdio_init_all();
    init_pins();
    init_display();
    detect_fall();
    check_emergency_a_button(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL);
    monitor_activity();
    void monitor_noise();

    // Inicializa o ADC
    adc_init();
    adc_gpio_init(MIC_PIN);  // Configura GPIO28 como entrada ADC para o microfone

    // Define o intervalo entre amostras (em microsegundos)
    uint64_t intervalo_us = 1000000 / amostras_por_segundo;

    //loop infinito
    while (true) {
        
         // Lê o valor do microfone (canal 2)
        adc_select_input(2);          // Seleciona o canal 2 (GPIO28)
        uint16_t mic_value = adc_read(); // Lê o ADC

        // Controle dos LEDs baseado no valor do microfone
        if ( mic_value > limiar_1)  {
            gpio_put(BUZZER_PIN, 1);
            gpio_put(LED_GREEN_PIN, 1); // Desliga o LED vermelho
            
            //mostrar mensagem de emergência no display
            printf("Nível de ruído: %d\n", mic_value);
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
            ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
            ssd1306_draw_string(&ssd,"SISTEMA ATIVO", 12, 10);
            ssd1306_draw_string(&ssd, "RUIDO ALTO", 20, 35);
            ssd1306_send_data(&ssd); // Atualiza o display
        } else {
            gpio_put(BUZZER_PIN, 0);
            gpio_put(LED_GREEN_PIN, 0); // Desliga o LED vermelho

            //limpar o display e retorna para o inicio.
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            init_display();

        }

        // Delay para alcançar a frequência de amostragem desejada
        sleep_us(intervalo_us);
        
    }    
}




//Funções

// Função para inicializar os pinos
uint init_pins(){
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &check_emergency_a_button);

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
    adc_gpio_init(MIC_PIN);
}


// Função para inicializar o display OLED
void init_display(){
    // Inicializa a interface I2C
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(DISPLAY_SDA_PIN);
    gpio_pull_up(DISPLAY_SCL_PIN);

    // Inicializa o display OLED
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Exibe a mensagem "Sistema Ativo"
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
    ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
    ssd1306_draw_string(&ssd,"SISTEMA ATIVO", 12, 10);
    ssd1306_send_data(&ssd); // Atualiza o display
}

// Função para detectar quedas usando o joystick analógico
void detect_fall(){
    uint16_t prev_x = 0, prev_y = 0;
    uint16_t threshold = 100; 

    while(1){
    adc_select_input(0); // Joystick X
    uint16_t x = adc_read();
    adc_select_input(1); // Joystick Y
    uint16_t y = adc_read();

        if(abs(x - prev_x) > threshold || abs(y - prev_y) > threshold){
        
            if(x > 3000 || x < 1000 || y > 3000 || y < 1000){
                gpio_put(BUZZER_PIN, 1);
                gpio_put(LED_RED_PIN, 1);
                printf("Joystick X: %d\n", x);
                printf("Joystick y: %d\n", y);
                ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
                ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
                ssd1306_draw_string(&ssd,"SISTEMA ATIVO", 12, 10);
                ssd1306_draw_string(&ssd,"DETECCAO ", 20, 35);
                ssd1306_draw_string(&ssd,"DE QUEDA", 30, 45);
                ssd1306_send_data(&ssd); // Atualiza o display
                sleep_ms(3000);
                gpio_put(BUZZER_PIN, 0);
                gpio_put(LED_RED_PIN,0);
                ssd1306_fill(&ssd, false);
                ssd1306_send_data(&ssd);
                init_display();
                }
        }
        sleep_ms(100);
    }
    
}


// Função para verificar o botão de emergência
void check_emergency_a_button(uint gpio, uint32_t events){
   
    if (!emergencia_acionada) {
        emergencia_acionada = true;
        gpio_put(LED_BLUE_PIN, 1);
        gpio_put(BUZZER_PIN, 1);

        //mostrar mensagem de emergência no display
        printf("Emergência!\n");
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
        ssd1306_draw_string(&ssd,"SISTEMA ATIVO", 12, 10);
        ssd1306_draw_string(&ssd, "EMERGENCIA", 20, 35);
        ssd1306_send_data(&ssd); // Atualiza o display
        

        // Inicia o temporizador para desligar o LED verde após 3 segundos
        add_alarm_in_ms(5000, turn_off_blue_led, NULL, false);
    }
    
   
}

// Função de callback para desligar o LED azul
int64_t turn_off_blue_led(alarm_id_t id, void *user_data) {
    gpio_put(LED_BLUE_PIN, 0);
    gpio_put(BUZZER_PIN, 0);

    //limpar o display e retorna para o inicio.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    init_display();

    emergencia_acionada = false; // Permitir que o botão seja pressionado novamente
    return false; // Não repetir o temporizador
}


// Função para monitorar a atividade usando o joystick analógico
void monitor_activity(){
    adc_select_input(0);
    uint16_t x = adc_read();
    adc_select_input(1);
    uint16_t y = adc_read();

    if(x > 1000 || y > 1000){
        //mostrar padrão na matriz de leds
        printf("Atividade detectada\n");
        ssd1306_draw_string(&ssd, "ATIVIDADE DETECTADA", 20, 16);
        ssd1306_send_data(&ssd); // Atualiza o display
        sleep_ms(5000);
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);
        init_display();
    }
}

// Função para monitorar o ruído usando o microfone
void monitor_ise(){
    adc_select_input(2);
    uint16_t noise = adc_read();

    if(noise > 200){
        gpio_put(BUZZER_PIN, 1);
        printf("Ruído detectado\n");
        ssd1306_draw_string(&ssd, "RUIDO DETECTADO", 20, 16);
        ssd1306_send_data(&ssd); // Atualiza o display
        sleep_ms(5000);
        gpio_put(BUZZER_PIN, 0);
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);
        init_display();
    }
}

void monitor_noise() {
  // Define o intervalo entre amostras (em microsegundos)
    uint64_t intervalo_us = 1000000 / amostras_por_segundo;

    while (true) {
         // Lê o valor do microfone (canal 2)
        adc_select_input(2);          // Seleciona o canal 2 (GPIO28)
        uint16_t mic_value = adc_read(); // Lê o ADC

        // Controle dos LEDs baseado no valor do microfone
        if ( mic_value > limiar_1)  {
            gpio_put(BUZZER_PIN, 1);
            gpio_put(LED_GREEN_PIN, 1); // Desliga o LED vermelho
            
            //mostrar mensagem de emergência no display
            printf("Nível de ruído: %d\n", mic_value);
            ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
            ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
            ssd1306_draw_string(&ssd,"SISTEMA ATIVO", 12, 10);
            ssd1306_draw_string(&ssd, "RUIDO ALTO", 20, 35);
            ssd1306_send_data(&ssd); // Atualiza o display
        } else {
            gpio_put(BUZZER_PIN, 0);
            gpio_put(LED_GREEN_PIN, 0); // Desliga o LED vermelho

            //limpar o display e retorna para o inicio.
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            init_display();

        }



        

        // Adicione um pequeno atraso para evitar leituras muito rápidas
        sleep_ms(intervalo_us);
    }
}

