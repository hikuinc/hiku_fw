exec-y += timer_demo
timer_demo-objs-y := src/main.c src/timer_demo.c
# Applications could also define custom board files if required using following:
#timer_demo-board-y := /path/to/boardfile
#timer_demo-linkerscrpt-y := /path/to/linkerscript
