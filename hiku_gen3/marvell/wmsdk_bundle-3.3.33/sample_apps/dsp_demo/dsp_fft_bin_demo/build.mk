exec-y += dsp_fft_bin_demo
dsp_fft_bin_demo-objs-y := src/main.c src/arm_fft_bin_data.c src/arm_fft_bin_example_f32.c

dsp_fft_bin_demo-prebuilt-libs-y += -lm

# Applications could also define custom board files if required using following:
#dsp_fft_bin_demo-board-y := /path/to/boardfile
#dsp_fft_bin_demo-linker-y := /path/to/linkerscript
