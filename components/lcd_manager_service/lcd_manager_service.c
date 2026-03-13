#include <stdio.h>
#include "lcd_manager_service.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2c_master.h"

#define LCD_COLS 16
#define LCD_ROWS 2

#define LCD_QUEUE_SIZE 10
#define LCD_HISTORY_SIZE 20
#define LCD_LINE_LEN (LCD_COLS + 1)
#define SLAVE_ADDRESS_LCD   (0x4E >> 1)

#define TAG "LCD"
#define I2C_MASTER_SCL_IO   GPIO_NUM_5
#define I2C_MASTER_SDA_IO   GPIO_NUM_4
#define I2C_MASTER_FREQ_HZ  400000


static bool lcd_available = false;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t lcd_dev = NULL;


typedef struct {
    char text[LCD_LINE_LEN];
} lcd_msg_t;

typedef struct {
    char lines[LCD_HISTORY_SIZE][LCD_LINE_LEN];
    int count;
    int head;
    int view_index;
    bool user_scrolling;
} lcd_history_t;

static QueueHandle_t lcd_queue;
static lcd_history_t lcd_hist;


static void lcd_hw_print(const char *cmd);


static bool lcd_probe(void)
{
    uint8_t dummy = 0x00;

    esp_err_t ret =
        i2c_master_transmit(lcd_dev, &dummy, 1, 50);

    return (ret == ESP_OK);
}

/* =========================================================
   LCD HARDWARE DRIVER (placeholder)
   Hubungkan ke driver LCD kamu
   ========================================================= */

static esp_err_t lcd_hw_init()
{
    // init I2C + LCD disini
    //
    esp_err_t ret;

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SLAVE_ADDRESS_LCD,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &lcd_dev);
    lcd_available = lcd_probe();

    if (!lcd_available)
    ESP_LOGW(TAG, "LCD not detected");


    return ret;
}


static esp_err_t lcd_i2c_write(uint8_t *data, size_t len)
{
    return i2c_master_transmit(lcd_dev, data, len, -1);
}



static void lcd_hw_send_cmd(uint8_t cmd)
{
    uint8_t data_u = cmd & 0xF0;
    uint8_t data_l = (cmd << 4) & 0xF0;

    uint8_t data_t[4] = {
        data_u | 0x0C,
        data_u | 0x08,
        data_l | 0x0C,
        data_l | 0x08
    };

    if (lcd_i2c_write(data_t, 4) != ESP_OK)
        ESP_LOGE(TAG, "CMD write error");
}


static void lcd_hw_set_cursor(uint8_t col, uint8_t row)
{
    // kirim command cursor ke LCD
    


    uint8_t addr = (row == 0) ? 0x80 : 0xC0;
    lcd_hw_send_cmd(addr | col);


}

static void lcd_hw_print(const char *cmd)
{
    uint8_t data = *cmd;
    // kirim string ke LCD
    



    uint8_t data_u = data & 0xF0;
    uint8_t data_l = (data << 4) & 0xF0;

    uint8_t data_t[4] = {
        data_u | 0x0D,
        data_u | 0x09,
        data_l | 0x0D,
        data_l | 0x09
    };

    if (lcd_i2c_write(data_t, 4) != ESP_OK)
        ESP_LOGE(TAG, "DATA write error");
    













}


/* =========================================================
   HISTORY MANAGEMENT
   ========================================================= */

static void lcd_add_history(const char *text)
{
    snprintf(
        lcd_hist.lines[lcd_hist.head],
        LCD_LINE_LEN,
        "%-16s",
        text
    );

    lcd_hist.head =
        (lcd_hist.head + 1) % LCD_HISTORY_SIZE;

    if (lcd_hist.count < LCD_HISTORY_SIZE)
        lcd_hist.count++;

    if (!lcd_hist.user_scrolling)
    {
        lcd_hist.view_index = lcd_hist.count - 2;

        if (lcd_hist.view_index < 0)
            lcd_hist.view_index = 0;
    }
}


/* =========================================================
   RENDER VIEWPORT
   ========================================================= */

static void lcd_render()
{
    int i1 = lcd_hist.view_index;
    int i2 = lcd_hist.view_index + 1;

    lcd_hw_set_cursor(0,0);
    lcd_hw_print(lcd_hist.lines[i1]);

    lcd_hw_set_cursor(0,1);

    if (i2 < lcd_hist.count)
        lcd_hw_print(lcd_hist.lines[i2]);
    else
        lcd_hw_print("                ");
}


/* =========================================================
   PUBLIC LOG API (THREAD SAFE)
   ========================================================= */

void lcd_log(const char *msg)
{
    if (!lcd_queue) return;

    lcd_msg_t item = {0};

    snprintf(
        item.text,
        LCD_LINE_LEN,
        "%s",
        msg
    );

    xQueueSend(lcd_queue, &item, 0);
}


/* =========================================================
   SCROLL CONTROL
   ========================================================= */

void lcd_scroll_up()
{
    lcd_hist.user_scrolling = true;

    if (lcd_hist.view_index > 0)
    {
        lcd_hist.view_index--;
        lcd_render();
    }
}

void lcd_scroll_down()
{
    lcd_hist.user_scrolling = true;

    if (lcd_hist.view_index < lcd_hist.count - 2)
    {
        lcd_hist.view_index++;
        lcd_render();
    }
}

void lcd_scroll_latest()
{
    lcd_hist.user_scrolling = false;

    lcd_hist.view_index = lcd_hist.count - 2;

    if (lcd_hist.view_index < 0)
        lcd_hist.view_index = 0;

    lcd_render();
}


/* =========================================================
   LCD TASK
   ========================================================= */

static void lcd_task(void *arg)
{
    lcd_msg_t msg;

    lcd_hw_init();

    while (1)
    {
        if (xQueueReceive(
                lcd_queue,
                &msg,
                portMAX_DELAY))
        {
            lcd_add_history(msg.text);

            lcd_render();
        }
    }
}


/* =========================================================
   INIT
   ========================================================= */

void lcd_manager_init()
{
    memset(&lcd_hist,0,sizeof(lcd_hist));

    lcd_queue =
        xQueueCreate(
            LCD_QUEUE_SIZE,
            sizeof(lcd_msg_t)
        );

    xTaskCreate(
        lcd_task,
        "lcd_task",
        4096,
        NULL,
        5,
        NULL
    );
}


/* =========================================================
   EXAMPLE USAGE
   ========================================================= */

void example_usage()
{
    lcd_manager_init();

    lcd_log("Booting...");
    lcd_log("WiFi Connecting");
    lcd_log("WiFi Connected");
    lcd_log("MQTT Ready");
}


/* =========================================================
   COMMAND EXAMPLE (FROM PHONE / MQTT)
   ========================================================= */

void handle_command(const char *cmd)
{
    if(strcmp(cmd,"UP")==0)
        lcd_scroll_up();

    if(strcmp(cmd,"DOWN")==0)
        lcd_scroll_down();

    if(strcmp(cmd,"LATEST")==0)
        lcd_scroll_latest();
}
