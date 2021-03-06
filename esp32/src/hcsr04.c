/**
 *
 * Based on "NEC remote infrared RMT example"
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt_nec_tx_rx
 *
 */
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/rmt.h>
#include <driver/periph_ctrl.h>
#include <soc/rmt_reg.h>


static SemaphoreHandle_t m_access_mutex;
static float m_hcsr04_distance_cm = 0.0;



static const char* M_TAG = "HCSR04";


#define RMT_TX_CHANNEL    1     /*!< RMT channel for transmitter */
#define RMT_TX_GPIO_NUM  18     /*!< GPIO number for transmitter signal */
#define RMT_RX_CHANNEL    0     /*!< RMT channel for receiver */
#define RMT_RX_GPIO_NUM  19     /*!< GPIO number for receiver */

#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */

#define HCSR04_MAX_TIMEOUT_US  25000   /*!< RMT receiver timeout value(us) */


#define US2TICKS(us)	(us / 10 * RMT_TICK_10_US)
#define TICKS2US(ticks)	(ticks * 10 / RMT_TICK_10_US)


static inline void set_item_edge(rmt_item32_t* item, int low_us, int high_us)
{
    item->level0 = 0;
    item->duration0 = US2TICKS(low_us);
    item->level1 = 1;
    item->duration1 = US2TICKS(high_us);
}


/*
 * @brief RMT transmitter initialization
 */
static void nec_tx_init()
{
    rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;

    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = 0;	// off

    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);
}


/*
 * @brief RMT receiver initialization
 */
static void nec_rx_init()
{
    rmt_config_t rmt_rx;
    rmt_rx.channel = RMT_RX_CHANNEL;
    rmt_rx.gpio_num = RMT_RX_GPIO_NUM;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = false;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = US2TICKS(HCSR04_MAX_TIMEOUT_US);
    rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 1000, 0);
}


/**
 * @brief RMT receiver demo, this task will print each received NEC data.
 *
 */
static void rx_task()
{
    int channel = RMT_RX_CHANNEL;

    nec_rx_init();

    RingbufHandle_t rb = NULL;

    //get RMT RX ringbuffer
    rmt_get_ringbuf_handle(channel, &rb);
    rmt_rx_start(channel, 1);

    while (rb)
	{
        size_t rx_size = 0;
        //try to receive data from ringbuffer.
        //RMT driver will push all the data it receives to its ringbuffer.
        //We just need to parse the value and return the spaces of ringbuffer.
        rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, 1000);
        if (item)
        {
        	for (int i=0; i<rx_size / sizeof(rmt_item32_t); ++i)
        	{
				xSemaphoreTake(m_access_mutex, portMAX_DELAY);
        		m_hcsr04_distance_cm = (float)TICKS2US((item[i].duration0) / 58.2);
        		xSemaphoreGive(m_access_mutex);

        		ESP_LOGD(M_TAG, "RMT RCV -- %d:%d | %d:%d : %.1fcm",
						item[i].level0, TICKS2US(item[i].duration0),
						item[i].level1, TICKS2US(item[i].duration1),
						m_hcsr04_distance_cm);
        	}
            //after parsing the data, return spaces to ringbuffer.
            vRingbufferReturnItem(rb, (void*) item);
        } else {
			vTaskDelay(10);
//            break;
        }
    }

    vTaskDelete(NULL);
}


/**
 * @brief RMT transmitter demo, this task will periodically send NEC data. (100 * 32 bits each time.)
 *
 */
static void tx_task()
{
    vTaskDelay(10);
    nec_tx_init();

    int channel = RMT_TX_CHANNEL;

    int item_num = 1;
	rmt_item32_t item[item_num];
	for (int i=0; i<item_num; ++i)
		set_item_edge(&item[i], 20, 180);
//	set_item_edge(&item[1], factor * 70, factor * 30);

    for (;;)
	{
//        ESP_LOGI(M_TAG, "RMT TX DATA");

        // To send data according to the waveform items.
        rmt_write_items(channel, item, item_num, true);
        // Wait until sending is done.
        rmt_wait_tx_done(channel, portMAX_DELAY);
        // before we free the data, make sure sending is already done.

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}


float hcsr04_get_distance()
{
	xSemaphoreTake(m_access_mutex, portMAX_DELAY);
	float val = m_hcsr04_distance_cm;
	xSemaphoreGive(m_access_mutex);
	return val;
}


void hcsr04_init()
{
	m_access_mutex = xSemaphoreCreateMutex();
    xTaskCreate(rx_task, "rx_task", 2048, NULL, 10, NULL);
    xTaskCreate(tx_task, "tx_task", 2048, NULL, 10, NULL);
}
