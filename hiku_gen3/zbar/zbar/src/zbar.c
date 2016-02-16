#include <stdio.h>
#include <stdlib.h>
#include <lodepng.h>
#include <zbar.h>

zbar_image_scanner_t *scanner = NULL;

int main (void)
{
    //if(argc < 2) return(1);
	char * filename = "pepsicancode.png";
    /* create a reader */
    scanner = zbar_image_scanner_create();

    /* configure the reader */
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    /* obtain image data */
    int width = 0, height = 0;
    void *raw = NULL;

    /* declare LodePNG related vars */
    unsigned error;
    unsigned char* rawimage;
    unsigned char* png = 0;
    size_t pngsize;
    LodePNGState state;

    /* customize the state to give us 8bit Greyscale or Y800 expected by ZBAR*/
    lodepng_state_init(&state);
    state.info_raw.bitdepth = 8;
    state.info_raw.colortype = LCT_GREY;

    /* load the png file and get the rawimage data*/
    error = lodepng_load_file(&png, &pngsize, filename);
    if(!error) error = lodepng_decode(&rawimage, &width, &height, &state, png, pngsize);
    if(error) printf("error %u: %s\n", error, lodepng_error_text(error));
    free(png);

    lodepng_state_cleanup(&state);
    //free(image);

    /* encode it back into a file to see if it's grayscale - uses only R values according
     * to lodepng documentation */
    unsigned error2 = lodepng_encode(&png, &pngsize, rawimage, width, height, &state);
    if(!error2) lodepng_save_file(png, pngsize, "encode.png");

    /* print raw data for debug */
    //int i=0;
    //for (i=0;i<(width*height);i++){
    //	printf("%x/", *rawimage);
    //	rawimage++;
    //}

    /* wrap image data */
    zbar_image_t *image = zbar_image_create();
    zbar_image_set_format(image, zbar_fourcc('Y','8','0','0'));
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, rawimage, width * height, zbar_image_free_data);

    /* scan the image for barcodes */
    int n = zbar_scan_image(scanner, image);

    /* extract results */
    const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* print the results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        printf("decoded %s symbol \"%s\"\n",
               zbar_get_symbol_name(typ), data);
    }

    /* clean up */
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);
    //free(rawimage);
    return(0);
}
