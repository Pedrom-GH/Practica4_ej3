#include <Arduino.h>

// Definición de pines [cite: 221-223]
#define LED_SEGUNDOS 2
#define LED_MODO 4
#define BTN_MODO 16
#define BTN_INCREMENTO 17

// Variables globales protegidas por Mutex [cite: 225-228]
volatile int horas = 0, minutos = 0, segundos = 0;
volatile int modo = 0; // 0: normal, 1: ajuste horas, 2: ajuste minutos

// Recursos de FreeRTOS [cite: 230-231]
QueueHandle_t botonQueue;
SemaphoreHandle_t relojMutex;

// Estructura para eventos de botones [cite: 233-236]
typedef struct {
    uint8_t boton;
    uint32_t tiempo;
} EventoBoton;

// ISR: Se ejecuta al presionar un botón [cite: 242, 451-454]
void IRAM_ATTR ISR_Boton(void *arg) {
    uint8_t numBoton = (uint32_t)arg;
    EventoBoton evento = {numBoton, xTaskGetTickCountFromISR()};
    xQueueSendFromISR(botonQueue, &evento, NULL);
}

// Tarea: Maneja el paso del tiempo cada segundo [cite: 312-329, 458-470]
void TareaReloj(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
            if (modo == 0) { // Solo cuenta en modo normal
                segundos++;
                if (segundos >= 60) { segundos = 0; minutos++; }
                if (minutos >= 60) { minutos = 0; horas++; }
                if (horas >= 24) { horas = 0; }
            }
            xSemaphoreGive(relojMutex);
        }
    }
}

// Tarea: Procesa los botones desde la cola [cite: 343-379, 471-473]
void TareaBotones(void *pvParameters) {
    EventoBoton evento;
    while (xQueueReceive(botonQueue, &evento, portMAX_DELAY)) {
        if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
            if (evento.boton == BTN_MODO) {
                modo = (modo + 1) % 3;
            } else if (evento.boton == BTN_INCREMENTO) {
                if (modo == 1) horas = (horas + 1) % 24;
                else if (modo == 2) minutos = (minutos + 1) % 60;
            }
            xSemaphoreGive(relojMutex);
        }
    }
}

// Tarea: Muestra la hora en el Serial [cite: 381-416, 473]
void TareaDisplay(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(relojMutex, portMAX_DELAY) == pdTRUE) {
            Serial.printf("HORA: %02d:%02d:%02d | MODO: %d\n", horas, minutos, segundos, modo);
            digitalWrite(LED_MODO, modo > 0);
            xSemaphoreGive(relojMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_SEGUNDOS, OUTPUT); 
    pinMode(LED_MODO, OUTPUT);
    pinMode(BTN_MODO, INPUT_PULLUP);
    pinMode(BTN_INCREMENTO, INPUT_PULLUP);

    relojMutex = xSemaphoreCreateMutex(); // [cite: 261, 449]
    botonQueue = xQueueCreate(10, sizeof(EventoBoton)); // [cite: 260, 449]

    attachInterruptArg(BTN_MODO, ISR_Boton, (void*)BTN_MODO, FALLING); // [cite: 263]
    attachInterruptArg(BTN_INCREMENTO, ISR_Boton, (void*)BTN_INCREMENTO, FALLING); // [cite: 263]

    xTaskCreate(TareaReloj, "Reloj", 2048, NULL, 1, NULL); // [cite: 265-272]
    xTaskCreate(TareaBotones, "Botones", 2048, NULL, 2, NULL); // [cite: 279-286]
    xTaskCreate(TareaDisplay, "Display", 2048, NULL, 1, NULL); // [cite: 289-296]
}

void loop() {} // El loop se queda vacío en aplicaciones RTOS [cite: 306-309]