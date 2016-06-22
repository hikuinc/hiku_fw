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

 uint8_t zbar_hiku_process(char *barcode_val, char *barcode_typ){

			uint8_t barcode_matches = 0;

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
				
				barcode_matches++;

				zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
				volatile const char *data = zbar_symbol_get_data(symbol);
	
				strcpy(barcode_val, data);
				strcpy(barcode_typ, (uint8_t *)zbar_get_symbol_name(typ));

			}

			return barcode_matches;
			
}