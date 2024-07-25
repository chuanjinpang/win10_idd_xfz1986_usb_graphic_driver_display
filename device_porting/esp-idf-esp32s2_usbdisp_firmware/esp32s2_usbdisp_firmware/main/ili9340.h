#ifndef MAIN_ILI9340_H_
#define MAIN_ILI9340_H_

#include "driver/spi_master.h"

#define RED			0xf800
#define GREEN			0x07e0
#define BLUE			0x001f
#define BLACK			0x0000
#define WHITE			0xffff
#define GRAY			0x8c51
#define YELLOW			0xFFE0
#define CYAN			0x07FF
#define PURPLE			0xF81F


#define DIRECTION0		0
#define DIRECTION90		1
#define DIRECTION180		2
#define DIRECTION270		3

typedef struct {
    uint16_t _model;
    uint16_t _width;
    uint16_t _height;
    uint16_t _offsetx;
    uint16_t _offsety;
    uint16_t _font_direction;
    uint16_t _font_fill;
    uint16_t _font_fill_color;
    uint16_t _font_underline;
    uint16_t _font_underline_color;
    int16_t _dc;
    int16_t _bl;
    spi_device_handle_t _SPIHandle;
} TFT_t;

void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC, int16_t GPIO_RESET, int16_t GPIO_BL);
bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength);
bool spi_master_write_comm_byte(TFT_t * dev, uint8_t cmd);
bool spi_master_write_comm_word(TFT_t * dev, uint16_t cmd);
bool spi_master_write_data_byte(TFT_t * dev, uint8_t data);
bool spi_master_write_data_word(TFT_t * dev, uint16_t data);
bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2);
bool spi_master_write_color(TFT_t * dev, uint16_t color, uint16_t size);
bool spi_master_write_colors(TFT_t * dev, uint16_t * colors, uint16_t size);

void delayMS(int ms);
void lcdWriteRegisterWord(TFT_t * dev, uint16_t addr, uint16_t data);
void lcdWriteRegisterByte(TFT_t * dev, uint8_t addr, uint16_t data);
void lcdInit(TFT_t * dev, uint16_t model, int width, int height, int offsetx, int offsety);
void lcdBGRFilter(TFT_t * dev);
void lcd_bitblt(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t * colors) ;
void lcd_bitblt_dma(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t * colors);

#endif /* MAIN_ILI9340_H_ */

