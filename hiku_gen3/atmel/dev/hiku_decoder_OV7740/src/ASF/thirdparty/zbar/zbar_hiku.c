/*
 * zbar_hiku.c
 *
 * Created: 6/22/2016 12:32:55 PM
 *  Author: emran
 */ 
 //#include <config.h>

 #include "asf.h"
 #include <string.h>     /* memmove */
 #include <stdint.h>
 #include <zbar.h>
 #include <conf_board.h>

 void zbar_hiku_process(void){

			zbar_image_scanner_t *scanner = NULL;
			scanner = zbar_image_scanner_create();
			zbar_image_scanner_set_config(scanner, ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
	
			uint8_t * temp = (uint8_t *)CAP_DEST;
			zbar_image_t *image = zbar_image_create();
			zbar_image_set_format(image, zbar_fourcc('G','R','E','Y'));
			zbar_image_set_size(image, IMAGE_WIDTH, IMAGE_HEIGHT);
			zbar_image_set_data(image, temp, IMAGE_WIDTH * IMAGE_HEIGHT, zbar_image_free_data);

			int n = zbar_scan_image(scanner, image);

			const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
			for(; symbol; symbol = zbar_symbol_next(symbol)) {
				
//				g_ul_end_process_time = time_tick_get();

				char capture_time[32];
				char process_time[32];
				char total_time[32];
				
//				sprintf(capture_time, "%u ms", g_ul_end_capture_time - g_ul_begin_capture_time);
//				sprintf(process_time, "%u ms", g_ul_end_process_time - g_ul_begin_process_time);
//				sprintf(total_time, "%u ms", time_tick_get() - g_ul_begin_capture_time);

				zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
				volatile const char *data = zbar_symbol_get_data(symbol);
	
				//printf("decoded %s symbol \"%s\"\n", zbar_get_symbol_name(typ), data);
//				display_init();
				ili9325_fill(COLOR_BLUE);
				ili9325_draw_string(0, 20, (uint8_t *)data);
				ili9325_draw_string(0, 80, (uint8_t *)zbar_get_symbol_name(typ));
				ili9325_draw_string(0, 100, (uint8_t *)capture_time );
				ili9325_draw_string(0, 120, (uint8_t *)process_time );
				ili9325_draw_string(0, 140, (uint8_t *)total_time );

				delay_ms(3000);
//				g_ul_push_button_trigger = false;


			}
			
}