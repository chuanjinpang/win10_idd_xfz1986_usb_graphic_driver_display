

pixel_type_t lcd_color_test_pattern[640 * 480] = { 0 };

#define TEST_COLOR_RED 0xff
#define TEST_COLOR_GREEN 0xff00
#define TEST_COLOR_BLUE 0xff0000

void fill_color(pixel_type_t *buf, int len, pixel_type_t color)
{
	int i = 0;
	for (i = 0; i <= len; i++) {
		buf[i] = color;
	}
	//dump_memory_bytes("-",buf,16);
}


void fill_color_bar(pixel_type_t *buf, int len)
{
#if 0
	fill_color(buf, len / 3, TEST_COLOR_RED);
#else
	fill_color(buf, len / 12, TEST_COLOR_RED);
	fill_color(&buf[len / 12], len / 12, TEST_COLOR_GREEN);
	fill_color(&buf[(len * 2) / 12], len / 12, TEST_COLOR_BLUE);
#endif
}

void fill_color565(uint16_t *buf, int len, uint16_t color)
{
	int i = 0;
	for (i = 0; i <= len; i++) {
		buf[i] = color;
	}
	//dump_memory_bytes("-",buf,16);
}
//rgb565  high5 is red  low 5bit blue
void fill_color565_bar(uint16_t *buf, int len)
{
#if 0
	fill_color565(buf, len / 3, TEST_COLOR_RED);
#else
	fill_color565(buf, len / 3, rgb565(0xff, 0, 0));
	fill_color565(&buf[len / 3], len / 3, rgb565(0, 0xff, 0));
	fill_color565(&buf[(len * 2) / 3], len / 3, rgb565(0, 0, 0xff));
#endif
}

