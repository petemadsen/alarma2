#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_log.h>

#include "config.h"
#include "leds.h"
#include "i2c.h"


static SemaphoreHandle_t m_access_mutex;
static uint8_t m_mode = 0;
static const char* M_TAG = "alarma/leds";


#define M_LEDS_OFF			0xff
#define M_LEDS_ON			0x00
#define M_TOP_GREEN_LEDS	0xf9
#define M_TOP_RED_LEDS		0xf6
#define M_TOP_LEFT_RED		(0xff & ~1)
#define M_TOP_LEFT_GREEN	(0xff & ~2)
#define M_TOP_RIGHT_GREEN	(0xff & ~4)
#define M_TOP_RIGHT_RED		(0xff & ~8)
#define M_FRONT_LEDS		0x0f


void leds_mode(uint8_t mode)
{
	xSemaphoreTake(m_access_mutex, portMAX_DELAY);
	m_mode = mode;
	xSemaphoreGive(m_access_mutex);
}


static inline void i2c_send(uint8_t val)
{
	i2c_master_write_slave(MY_LEDS_I2C_ADDR, &val, 1);
}


static uint8_t get_mode()
{
	uint8_t mode;
	xSemaphoreTake(m_access_mutex, portMAX_DELAY);
	mode = m_mode;
	xSemaphoreGive(m_access_mutex);
	return mode;
}


void leds_task(void* pvParameter)
{
	uint8_t prev_mode = -1;
	uint8_t mode = -1;
	for (;;)
	{
		prev_mode = mode;

		xSemaphoreTake(m_access_mutex, portMAX_DELAY);
		mode = m_mode;
		xSemaphoreGive(m_access_mutex);

		if (prev_mode == mode)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			continue;
		}

		switch (m_mode)
		{
		case MY_LEDS_MODE_OFF:
			ESP_LOGI(M_TAG, "mode=OFF");
			i2c_send(0xff);
			break;
		case MY_LEDS_MODE_ON:
			i2c_send(0x00);
			break;
		case MY_LEDS_MODE_ERROR:
			for (int i=0; i<2; ++i)
			{
				i2c_send(M_LEDS_OFF);
				vTaskDelay(200 / portTICK_PERIOD_MS);
				i2c_send(M_LEDS_ON);
				vTaskDelay(200 / portTICK_PERIOD_MS);
			}
			leds_mode(MY_LEDS_MODE_OFF);
			break;
		case MY_LEDS_MODE_ACTIVATED:
			i2c_send(M_TOP_GREEN_LEDS);	// beide grüne oben an
			break;
		case MY_LEDS_MODE_INPUT:
			i2c_send(M_TOP_RED_LEDS);
			break;
		case MY_LEDS_MODE_ALARM:
			while (mode == get_mode())
			{
				i2c_send(M_LEDS_OFF);
				vTaskDelay(200 / portTICK_PERIOD_MS);
				i2c_send(M_LEDS_ON);
				vTaskDelay(200 / portTICK_PERIOD_MS);
			}
			break;
		case MY_LEDS_MODE_DEMO:
			i2c_send(M_TOP_LEFT_RED);
			vTaskDelay(200 / portTICK_PERIOD_MS);
			i2c_send(M_TOP_LEFT_GREEN);
			vTaskDelay(200 / portTICK_PERIOD_MS);
			i2c_send(M_TOP_RIGHT_GREEN);
			vTaskDelay(200 / portTICK_PERIOD_MS);
			i2c_send(M_TOP_RIGHT_RED);
			vTaskDelay(200 / portTICK_PERIOD_MS);
			i2c_send(M_LEDS_OFF);
			break;
		}
	}
}

void leds_init()
{
	m_access_mutex = xSemaphoreCreateMutex();

	xTaskCreate(&leds_task, "leds_task", 2048, NULL, 5, NULL);
}
