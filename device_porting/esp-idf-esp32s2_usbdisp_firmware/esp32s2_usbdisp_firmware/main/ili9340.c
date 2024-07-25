#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"


#include "ili9340.h"

#define TAG "ILI9340"
#define	_DEBUG_ 0

#ifdef CONFIG_IDF_TARGET_ESP32
#define LCD_HOST HSPI_HOST
#elif defined CONFIG_IDF_TARGET_ESP32S2
#define LCD_HOST SPI2_HOST
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define LCD_HOST SPI2_HOST
#endif

//static const int GPIO_MOSI = 23;
//static const int GPIO_SCLK = 18;

static const int SPI_Command_Mode = 0;
static const int SPI_Data_Mode = 1;
//static const int SPI_Frequency = SPI_MASTER_FREQ_20M;
////static const int SPI_Frequency = SPI_MASTER_FREQ_26M;
static const int SPI_Frequency = SPI_MASTER_FREQ_40M;
//static const int SPI_Frequency = SPI_MASTER_FREQ_80M;


void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC, int16_t GPIO_RESET, int16_t GPIO_BL)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "GPIO_CS=%d", GPIO_CS);
    gpio_pad_select_gpio(GPIO_CS);
    gpio_set_direction(GPIO_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_CS, 0);

    ESP_LOGI(TAG, "GPIO_DC=%d", GPIO_DC);
    gpio_pad_select_gpio(GPIO_DC);
    gpio_set_direction(GPIO_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_DC, 0);

    ESP_LOGI(TAG, "GPIO_RESET=%d", GPIO_RESET);
    if(GPIO_RESET >= 0) {
        gpio_pad_select_gpio(GPIO_RESET);
        gpio_set_direction(GPIO_RESET, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_RESET, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(GPIO_RESET, 1);
    }

    ESP_LOGI(TAG, "GPIO_BL=%d", GPIO_BL);
    if(GPIO_BL >= 0) {
        gpio_pad_select_gpio(GPIO_BL);
        gpio_set_direction(GPIO_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_BL, 0);
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = GPIO_SCLK,
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 240 * 2 * 8
    };

    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_LOGD(TAG, "spi_bus_initialize=%d", ret);
    assert(ret == ESP_OK);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_Frequency,
        .spics_io_num = GPIO_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    spi_device_handle_t handle;
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &handle);
    ESP_LOGD(TAG, "spi_bus_add_device=%d", ret);
    assert(ret == ESP_OK);
    dev->_dc = GPIO_DC;
    dev->_bl = GPIO_BL;
    dev->_SPIHandle = handle;
}


bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength)
{
    spi_transaction_t SPITransaction;
    esp_err_t ret;

    if(DataLength > 0) {
        memset(&SPITransaction, 0, sizeof(spi_transaction_t));
        SPITransaction.length = DataLength * 8;
        SPITransaction.tx_buffer = Data;


#if 1
        ret = spi_device_transmit(SPIHandle, &SPITransaction);
#endif

        assert(ret == ESP_OK);
    }

    return true;
}

static int spi_dma_busy_check = 0;

bool spi_master_write_byte_dma(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength)
{
    static	spi_transaction_t SPITransaction;//the trans will alive in all transfer
    esp_err_t ret;

    if(DataLength > 0) {
        memset(&SPITransaction, 0, sizeof(spi_transaction_t));
        SPITransaction.length = DataLength * 8;
        SPITransaction.tx_buffer = Data;

#if 1
//use DMA mode
        ret = spi_device_queue_trans(SPIHandle, &SPITransaction, pdMS_TO_TICKS(100));  //portMAX_DELAY

#endif

        assert(ret == ESP_OK);

        spi_dma_busy_check++;
    }

    return true;
}


/***************************************************************************************
** Function name:           dmaWait
** Description:             Wait until DMA is over (blocking!)
***************************************************************************************/
void spi_dma_wait(spi_device_handle_t SPIHandle)
{
    if(!spi_dma_busy_check) return;
    spi_transaction_t *rtrans;
    esp_err_t ret;
#if 0
    ESP_LOGI(__FUNCTION__, "A");
    fflush(stdout);
#endif
    for(int i = 0; i < spi_dma_busy_check; ++i) {
        ret = spi_device_get_trans_result(SPIHandle, &rtrans, pdMS_TO_TICKS(100));
        assert(ret == ESP_OK);
    }

    spi_dma_busy_check = 0;
}


bool spi_master_write_comm_byte(TFT_t * dev, uint8_t cmd)
{
    static uint8_t Byte = 0;
    Byte = cmd;
    gpio_set_level(dev->_dc, SPI_Command_Mode);
    return spi_master_write_byte(dev->_SPIHandle, &Byte, 1);
}

bool spi_master_write_comm_word(TFT_t * dev, uint16_t cmd)
{
    static uint8_t Byte[2];
    Byte[0] = (cmd >> 8) & 0xFF;
    Byte[1] = cmd & 0xFF;
    gpio_set_level(dev->_dc, SPI_Command_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, 2);
}


bool spi_master_write_data_byte(TFT_t * dev, uint8_t data)
{
    static uint8_t Byte = 0;
    Byte = data;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, &Byte, 1);
}


bool spi_master_write_data_word(TFT_t * dev, uint16_t data)
{
    static uint8_t Byte[2];
    Byte[0] = (data >> 8) & 0xFF;
    Byte[1] = data & 0xFF;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, 2);
}

bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2)
{
    static uint8_t Byte[4];
    Byte[0] = (addr1 >> 8) & 0xFF;
    Byte[1] = addr1 & 0xFF;
    Byte[2] = (addr2 >> 8) & 0xFF;
    Byte[3] = addr2 & 0xFF;
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, Byte, 4);
}

bool spi_master_write_colors_bytes_dma(TFT_t * dev, uint8_t * colors, uint16_t size)
{
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte_dma(dev->_SPIHandle, colors, size);
}

bool spi_master_write_colors_bytes(TFT_t * dev, uint8_t * colors, uint16_t size)
{
    gpio_set_level(dev->_dc, SPI_Data_Mode);
    return spi_master_write_byte(dev->_SPIHandle, colors, size);
}

void delayMS(int ms)
{
    int _ms = ms + (portTICK_PERIOD_MS - 1);
    TickType_t xTicksToDelay = _ms / portTICK_PERIOD_MS;
    ESP_LOGD(TAG, "ms=%d _ms=%d portTICK_PERIOD_MS=%d xTicksToDelay=%d", ms, _ms, portTICK_PERIOD_MS, xTicksToDelay);
    vTaskDelay(xTicksToDelay);
}


void lcdWriteRegisterWord(TFT_t * dev, uint16_t addr, uint16_t data)
{
    spi_master_write_comm_word(dev, addr);
    spi_master_write_data_word(dev, data);
}


void lcdWriteRegisterByte(TFT_t * dev, uint8_t addr, uint16_t data)
{
    spi_master_write_comm_byte(dev, addr);
    spi_master_write_data_word(dev, data);
}



void lcdInit(TFT_t * dev, uint16_t model, int width, int height, int offsetx, int offsety)
{
    dev->_model = model;
    dev->_width = width;
    dev->_height = height;
    dev->_offsetx = offsetx;
    dev->_offsety = offsety;
    dev->_font_direction = DIRECTION0;
    dev->_font_fill = false;
    dev->_font_underline = false;

    if(dev->_model == 0x7789) {
        ESP_LOGI(TAG, "Your TFT is ST7789");
        ESP_LOGI(TAG, "Screen width:%d", width);
        ESP_LOGI(TAG, "Screen height:%d", height);
#if 1
		spi_master_write_comm_byte(dev, 0x01);	//Software Reset
		delayMS(10);

		spi_master_write_comm_byte(dev, 0x11);	//Sleep Out
		delayMS(100);
	
        spi_master_write_comm_byte(dev, 0x3a);	
        spi_master_write_data_byte(dev, 0x05);
  

        spi_master_write_comm_byte(dev, 0xB2);	
        spi_master_write_data_byte(dev, 0x0C);
        spi_master_write_data_byte(dev, 0x0C);
        spi_master_write_data_byte(dev, 0x00);
        spi_master_write_data_byte(dev, 0x33);
        spi_master_write_data_byte(dev, 0x33);

        spi_master_write_comm_byte(dev, 0xB7);	//Gate Control
        spi_master_write_data_byte(dev, 0x35);	
		
        spi_master_write_comm_byte(dev, 0xBB);	//VCOM Setting
        spi_master_write_data_byte(dev, 0x19);

        spi_master_write_comm_byte(dev, 0xC0);	
        spi_master_write_data_byte(dev, 0x2C);

        spi_master_write_comm_byte(dev, 0xC2);	
        spi_master_write_data_byte(dev, 0x01);

		spi_master_write_comm_byte(dev, 0xC4);	
		spi_master_write_data_byte(dev, 0x20);

		spi_master_write_comm_byte(dev, 0xC6);	
		spi_master_write_data_byte(dev, 0x0F);

        spi_master_write_comm_byte(dev, 0xD0);	
        spi_master_write_data_byte(dev, 0xA4);
        spi_master_write_data_byte(dev, 0xA1);


        spi_master_write_comm_byte(dev, 0xE0);	
spi_master_write_data_byte(dev, 0xD0);
spi_master_write_data_byte(dev, 0x04);
spi_master_write_data_byte(dev, 0x0D);
spi_master_write_data_byte(dev, 0x11);
spi_master_write_data_byte(dev, 0x13);
spi_master_write_data_byte(dev, 0x2B);
spi_master_write_data_byte(dev, 0x3F);

		spi_master_write_data_byte(dev, 0x54);
		spi_master_write_data_byte(dev, 0x4C);
		spi_master_write_data_byte(dev, 0x18);
		spi_master_write_data_byte(dev, 0x0D);
		spi_master_write_data_byte(dev, 0x0B);
		spi_master_write_data_byte(dev, 0x1F);
		spi_master_write_data_byte(dev, 0x23);


				spi_master_write_comm_byte(dev, 0xE1);	
		spi_master_write_data_byte(dev, 0xD0);
		spi_master_write_data_byte(dev, 0x04);
		spi_master_write_data_byte(dev, 0x0c);
		spi_master_write_data_byte(dev, 0x11);
		spi_master_write_data_byte(dev, 0x13);
		spi_master_write_data_byte(dev, 0x2c);
		spi_master_write_data_byte(dev, 0x3F);
		
				spi_master_write_data_byte(dev, 0x44);
				spi_master_write_data_byte(dev, 0x51);
				spi_master_write_data_byte(dev, 0x2f);
				spi_master_write_data_byte(dev, 0x1f);
				spi_master_write_data_byte(dev, 0x1f);
				spi_master_write_data_byte(dev, 0x20);
				spi_master_write_data_byte(dev, 0x23);

		//spi_master_write_comm_byte(dev, 0x21);	
		spi_master_write_comm_byte(dev, 0x13);	
		    delayMS(10);
        spi_master_write_comm_byte(dev, 0x29);	//Display ON
		#else
	spi_master_write_comm_byte(dev, 0x01);	//Software Reset
	delayMS(150);

	spi_master_write_comm_byte(dev, 0x11);	//Sleep Out
	delayMS(255);
	
	spi_master_write_comm_byte(dev, 0x3A);	//Interface Pixel Format
	spi_master_write_data_byte(dev, 0x05);
	delayMS(10);
	
	spi_master_write_comm_byte(dev, 0x36);	//Memory Data Access Control
	spi_master_write_data_byte(dev, 0x00);

	spi_master_write_comm_byte(dev, 0x2A);	//Column Address Set
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0xF0);

	spi_master_write_comm_byte(dev, 0x2B);	//Row Address Set
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0x00);
	spi_master_write_data_byte(dev, 0xF0);

	//spi_master_write_comm_byte(dev, 0x21);	//Display Inversion On
	//delayMS(10);

	spi_master_write_comm_byte(dev, 0x13);	//Normal Display Mode On
	delayMS(10);

	spi_master_write_comm_byte(dev, 0x29);	//Display ON
	delayMS(255);



		#endif
    } // endif 0x7796

    if(dev->_model == 0x9340 || dev->_model == 0x9341 || dev->_model == 0x7735) {
        if(dev->_model == 0x9340)
            ESP_LOGI(TAG, "Your TFT is ILI9340");
        if(dev->_model == 0x9341)
            ESP_LOGI(TAG, "Your TFT is ILI9341");
        if(dev->_model == 0x7735)
            ESP_LOGI(TAG, "Your TFT is ST7735");
        ESP_LOGI(TAG, "Screen width:%d", width);
        ESP_LOGI(TAG, "Screen height:%d", height);
        spi_master_write_comm_byte(dev, 0xC0);	//Power Control 1
        spi_master_write_data_byte(dev, 0x23);

        spi_master_write_comm_byte(dev, 0xC1);	//Power Control 2
        spi_master_write_data_byte(dev, 0x10);

        spi_master_write_comm_byte(dev, 0xC5);	//VCOM Control 1
        spi_master_write_data_byte(dev, 0x3E);
        spi_master_write_data_byte(dev, 0x28);

        spi_master_write_comm_byte(dev, 0xC7);	//VCOM Control 2
        spi_master_write_data_byte(dev, 0x86);

        spi_master_write_comm_byte(dev, 0x36);	//Memory Access Control
        spi_master_write_data_byte(dev, 0x08);	//Right top start, BGR color filter panel
        //spi_master_write_data_byte(dev, 0x00);//Right top start, RGB color filter panel

        spi_master_write_comm_byte(dev, 0x3A);	//Pixel Format Set
        spi_master_write_data_byte(dev, 0x55);	//65K color: 16-bit/pixel

        spi_master_write_comm_byte(dev, 0x20);	//Display Inversion OFF

        spi_master_write_comm_byte(dev, 0xB1);	//Frame Rate Control
        spi_master_write_data_byte(dev, 0x00);
        spi_master_write_data_byte(dev, 0x18);

        spi_master_write_comm_byte(dev, 0xB6);	//Display Function Control
        spi_master_write_data_byte(dev, 0x08);
        spi_master_write_data_byte(dev, 0xA2);	// REV:1 GS:0 SS:0 SM:0
        spi_master_write_data_byte(dev, 0x27);
        spi_master_write_data_byte(dev, 0x00);

        spi_master_write_comm_byte(dev, 0x26);	//Gamma Set
        spi_master_write_data_byte(dev, 0x01);

        spi_master_write_comm_byte(dev, 0xE0);	//Positive Gamma Correction
        spi_master_write_data_byte(dev, 0x0F);
        spi_master_write_data_byte(dev, 0x31);
        spi_master_write_data_byte(dev, 0x2B);
        spi_master_write_data_byte(dev, 0x0C);
        spi_master_write_data_byte(dev, 0x0E);
        spi_master_write_data_byte(dev, 0x08);
        spi_master_write_data_byte(dev, 0x4E);
        spi_master_write_data_byte(dev, 0xF1);
        spi_master_write_data_byte(dev, 0x37);
        spi_master_write_data_byte(dev, 0x07);
        spi_master_write_data_byte(dev, 0x10);
        spi_master_write_data_byte(dev, 0x03);
        spi_master_write_data_byte(dev, 0x0E);
        spi_master_write_data_byte(dev, 0x09);
        spi_master_write_data_byte(dev, 0x00);

        spi_master_write_comm_byte(dev, 0xE1);	//Negative Gamma Correction
        spi_master_write_data_byte(dev, 0x00);
        spi_master_write_data_byte(dev, 0x0E);
        spi_master_write_data_byte(dev, 0x14);
        spi_master_write_data_byte(dev, 0x03);
        spi_master_write_data_byte(dev, 0x11);
        spi_master_write_data_byte(dev, 0x07);
        spi_master_write_data_byte(dev, 0x31);
        spi_master_write_data_byte(dev, 0xC1);
        spi_master_write_data_byte(dev, 0x48);
        spi_master_write_data_byte(dev, 0x08);
        spi_master_write_data_byte(dev, 0x0F);
        spi_master_write_data_byte(dev, 0x0C);
        spi_master_write_data_byte(dev, 0x31);
        spi_master_write_data_byte(dev, 0x36);
        spi_master_write_data_byte(dev, 0x0F);

        spi_master_write_comm_byte(dev, 0x11);	//Sleep Out
        delayMS(120);

        spi_master_write_comm_byte(dev, 0x29);	//Display ON
    } // endif 0x9340/0x9341/0x7735

    if(dev->_model == 0x9225) {
        ESP_LOGI(TAG, "Your TFT is ILI9225");
        ESP_LOGI(TAG, "Screen width:%d", width);
        ESP_LOGI(TAG, "Screen height:%d", height);
        lcdWriteRegisterByte(dev, 0x10, 0x0000); // Set SAP,DSTB,STB
        lcdWriteRegisterByte(dev, 0x11, 0x0000); // Set APON,PON,AON,VCI1EN,VC
        lcdWriteRegisterByte(dev, 0x12, 0x0000); // Set BT,DC1,DC2,DC3
        lcdWriteRegisterByte(dev, 0x13, 0x0000); // Set GVDD
        lcdWriteRegisterByte(dev, 0x14, 0x0000); // Set VCOMH/VCOML voltage
        delayMS(40);

        // Power-on sequence
        lcdWriteRegisterByte(dev, 0x11, 0x0018); // Set APON,PON,AON,VCI1EN,VC
        lcdWriteRegisterByte(dev, 0x12, 0x6121); // Set BT,DC1,DC2,DC3
        lcdWriteRegisterByte(dev, 0x13, 0x006F); // Set GVDD
        lcdWriteRegisterByte(dev, 0x14, 0x495F); // Set VCOMH/VCOML voltage
        lcdWriteRegisterByte(dev, 0x10, 0x0800); // Set SAP,DSTB,STB
        delayMS(10);
        lcdWriteRegisterByte(dev, 0x11, 0x103B); // Set APON,PON,AON,VCI1EN,VC
        delayMS(50);

        lcdWriteRegisterByte(dev, 0x01, 0x011C); // set the display line number and display direction
        lcdWriteRegisterByte(dev, 0x02, 0x0100); // set 1 line inversion
        lcdWriteRegisterByte(dev, 0x03, 0x1030); // set GRAM write direction and BGR=1.
        lcdWriteRegisterByte(dev, 0x07, 0x0000); // Display off
        lcdWriteRegisterByte(dev, 0x08, 0x0808); // set the back porch and front porch
        lcdWriteRegisterByte(dev, 0x0B, 0x1100); // set the clocks number per line
        lcdWriteRegisterByte(dev, 0x0C, 0x0000); // CPU interface
        //lcdWriteRegisterByte(dev, 0x0F, 0x0D01); // Set Osc
        lcdWriteRegisterByte(dev, 0x0F, 0x0801); // Set Osc
        lcdWriteRegisterByte(dev, 0x15, 0x0020); // Set VCI recycling
        lcdWriteRegisterByte(dev, 0x20, 0x0000); // RAM Address
        lcdWriteRegisterByte(dev, 0x21, 0x0000); // RAM Address

        // Set GRAM area
        lcdWriteRegisterByte(dev, 0x30, 0x0000);
        lcdWriteRegisterByte(dev, 0x31, 0x00DB);
        lcdWriteRegisterByte(dev, 0x32, 0x0000);
        lcdWriteRegisterByte(dev, 0x33, 0x0000);
        lcdWriteRegisterByte(dev, 0x34, 0x00DB);
        lcdWriteRegisterByte(dev, 0x35, 0x0000);
        lcdWriteRegisterByte(dev, 0x36, 0x00AF);
        lcdWriteRegisterByte(dev, 0x37, 0x0000);
        lcdWriteRegisterByte(dev, 0x38, 0x00DB);
        lcdWriteRegisterByte(dev, 0x39, 0x0000);

        // Adjust GAMMA Curve
        lcdWriteRegisterByte(dev, 0x50, 0x0000);
        lcdWriteRegisterByte(dev, 0x51, 0x0808);
        lcdWriteRegisterByte(dev, 0x52, 0x080A);
        lcdWriteRegisterByte(dev, 0x53, 0x000A);
        lcdWriteRegisterByte(dev, 0x54, 0x0A08);
        lcdWriteRegisterByte(dev, 0x55, 0x0808);
        lcdWriteRegisterByte(dev, 0x56, 0x0000);
        lcdWriteRegisterByte(dev, 0x57, 0x0A00);
        lcdWriteRegisterByte(dev, 0x58, 0x0710);
        lcdWriteRegisterByte(dev, 0x59, 0x0710);

        lcdWriteRegisterByte(dev, 0x07, 0x0012);
        delayMS(50); // Delay 50ms
        lcdWriteRegisterByte(dev, 0x07, 0x1017);
    } // endif 0x9225

    if(dev->_model == 0x9226) {
        ESP_LOGI(TAG, "Your TFT is ILI9225G");
        ESP_LOGI(TAG, "Screen width:%d", width);
        ESP_LOGI(TAG, "Screen height:%d", height);
        //lcdWriteRegisterByte(dev, 0x01, 0x011c);
        lcdWriteRegisterByte(dev, 0x01, 0x021c);
        lcdWriteRegisterByte(dev, 0x02, 0x0100);
        lcdWriteRegisterByte(dev, 0x03, 0x1030);
        lcdWriteRegisterByte(dev, 0x08, 0x0808); // set BP and FP
        lcdWriteRegisterByte(dev, 0x0B, 0x1100); // frame cycle
        lcdWriteRegisterByte(dev, 0x0C, 0x0000); // RGB interface setting R0Ch=0x0110 for RGB 18Bit and R0Ch=0111for RGB16Bit
        lcdWriteRegisterByte(dev, 0x0F, 0x1401); // Set frame rate----0801
        lcdWriteRegisterByte(dev, 0x15, 0x0000); // set system interface
        lcdWriteRegisterByte(dev, 0x20, 0x0000); // Set GRAM Address
        lcdWriteRegisterByte(dev, 0x21, 0x0000); // Set GRAM Address
        //*************Power On sequence ****************//
        delayMS(50);
        lcdWriteRegisterByte(dev, 0x10, 0x0800); // Set SAP,DSTB,STB----0A00
        lcdWriteRegisterByte(dev, 0x11, 0x1F3F); // Set APON,PON,AON,VCI1EN,VC----1038
        delayMS(50);
        lcdWriteRegisterByte(dev, 0x12, 0x0121); // Internal reference voltage= Vci;----1121
        lcdWriteRegisterByte(dev, 0x13, 0x006F); // Set GVDD----0066
        lcdWriteRegisterByte(dev, 0x14, 0x4349); // Set VCOMH/VCOML voltage----5F60
        //-------------- Set GRAM area -----------------//
        lcdWriteRegisterByte(dev, 0x30, 0x0000);
        lcdWriteRegisterByte(dev, 0x31, 0x00DB);
        lcdWriteRegisterByte(dev, 0x32, 0x0000);
        lcdWriteRegisterByte(dev, 0x33, 0x0000);
        lcdWriteRegisterByte(dev, 0x34, 0x00DB);
        lcdWriteRegisterByte(dev, 0x35, 0x0000);
        lcdWriteRegisterByte(dev, 0x36, 0x00AF);
        lcdWriteRegisterByte(dev, 0x37, 0x0000);
        lcdWriteRegisterByte(dev, 0x38, 0x00DB);
        lcdWriteRegisterByte(dev, 0x39, 0x0000);
        // ----------- Adjust the Gamma Curve ----------//
        lcdWriteRegisterByte(dev, 0x50, 0x0001);
        lcdWriteRegisterByte(dev, 0x51, 0x200B);
        lcdWriteRegisterByte(dev, 0x52, 0x0000);
        lcdWriteRegisterByte(dev, 0x53, 0x0404);
        lcdWriteRegisterByte(dev, 0x54, 0x0C0C);
        lcdWriteRegisterByte(dev, 0x55, 0x000C);
        lcdWriteRegisterByte(dev, 0x56, 0x0101);
        lcdWriteRegisterByte(dev, 0x57, 0x0400);
        lcdWriteRegisterByte(dev, 0x58, 0x1108);
        lcdWriteRegisterByte(dev, 0x59, 0x050C);
        delayMS(50);
        lcdWriteRegisterByte(dev, 0x07, 0x1017);
    } // endif 0x9226

    if(dev->_bl >= 0) {
        gpio_set_level(dev->_bl, 1);
    }
}




// Add 202001
// Draw multi pixel
// x:X coordinate
// y:Y coordinate
// colors:colors
void lcd_bitblt_dma(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t * colors)
{
    uint16_t size = 0;
    if(x2 > dev->_width) return;
    if(y2 > dev->_height) return;

    size = (x2 - x1) * (y2 - y1) * 2; //0-239
    spi_dma_wait(dev->_SPIHandle);

    if(dev->_model == 0x9340 || dev->_model == 0x9341 ||dev->_model == 0x7789 ) {
        spi_master_write_comm_byte(dev, 0x2A);	// set column(x) address
        spi_master_write_addr(dev, x1, x2 - 1);
        spi_master_write_comm_byte(dev, 0x2B);	// set Page(y) address
        spi_master_write_addr(dev, y1, y2 - 1);
        spi_master_write_comm_byte(dev, 0x2C);	// Memory Write
        spi_master_write_colors_bytes_dma(dev, colors, size);
    }
}
void lcd_bitblt(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t * colors)
{
    uint16_t size = 0;
    if(x2 > dev->_width) return;
    if(y2 > dev->_height) return;
    size = (x2 - x1) * (y2 - y1) * 2; //0-239
#if 0
        ESP_LOGI(__FUNCTION__,"x1=%d x2=%d y1=%d y2=%d size:%d",x1, x2, y1, y2,size);
     fflush(stdout);
 #endif
    if(dev->_model == 0x9340 || dev->_model == 0x9341 || dev->_model == 0x7789) {
        spi_master_write_comm_byte(dev, 0x2A);	// set column(x) address
        spi_master_write_addr(dev, x1, x2 - 1);
        spi_master_write_comm_byte(dev, 0x2B);	// set Page(y) address
        spi_master_write_addr(dev, y1, y2 - 1);
        spi_master_write_comm_byte(dev, 0x2C);	// Memory Write
        spi_master_write_colors_bytes(dev, colors, size);
    }

}



// Display Inversion ON
void lcdInversionOn(TFT_t * dev)
{
    if(dev->_model == 0x9340 || dev->_model == 0x9341 || dev->_model == 0x7735 || dev->_model == 0x7796) {
        spi_master_write_comm_byte(dev, 0x21);
    } // endif 0x9340/0x9341/0x7735/0x7796

    if(dev->_model == 0x9225 || dev->_model == 0x9226) {
        lcdWriteRegisterByte(dev, 0x07, 0x1013);
    } // endif 0x9225/0x9226
}

// Change Memory Access Control
void lcdBGRFilter(TFT_t * dev)
{
    if(dev->_model == 0x9340 || dev->_model == 0x9341 || dev->_model == 0x7735 ) {
	ESP_LOGI(TAG, "Change LCD row colmn rgb order");
        spi_master_write_comm_byte(dev, 0x36);	//Memory Access Control
        spi_master_write_data_byte(dev, (1 << 5) | (1 << 6) | (1 << 3));	//Right top start, RGB color filter panel
    } // endif 0x9340/0x9341/0x7735/0x7796

    if(dev->_model == 0x7789) {
	ESP_LOGI(TAG, "7789Change LCD row colmn rgb order");
        spi_master_write_comm_byte(dev, 0x36);	//Memory Access Control
        spi_master_write_data_byte(dev, (1 << 5) | (1 << 7) );	
    } // endif 0x9225/0x9226
}

