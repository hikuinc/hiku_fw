exec-y += dsp_dotproduct_demo
dsp_dotproduct_demo-objs-y := src/main.c src/arm_dotproduct_example_f32.c

dsp_dotproduct_demo-prebuilt-libs-y += -lm

# Applications could also define custom board files if required using following:
#dsp_dotproduct_demo-board-y := /path/to/boardfile
#dsp_dotproduct_demo-linkerscript-y := /path/to/linkerscript
