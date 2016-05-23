exec-y += dsp_matrix_demo
dsp_matrix_demo-objs-y := src/main.c src/math_helper.c src/arm_matrix_example_f32.c

dsp_matrix_demo-cflgs-y += -Isrc/

dsp_matrix_demo-prebuilt-libs-y += -lm

# Applications could also define custom board files if required using following:
#dsp_matrix_demo-board-y := /path/to/boardfile
#dsp_matrix_demo-linkerscript-y := /path/to/linkerscript
