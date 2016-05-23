# Copyright (C) 2015 Marvell International Ltd.
# All Rights Reserved.

# Makefile: makefile for building CMSIS DSPLib

libs-y += libcmsis_dsp



# Directories for sources
DSPLBASE = $(DSPLROOT)/DSP_Lib/Source


DSPLDIRS =					\
	$(DSPLBASE)/BasicMathFunctions		\
	$(DSPLBASE)/CommonTables 		\
	$(DSPLBASE)/ComplexMathFunctions	\
	$(DSPLBASE)/ControllerFunctions		\
	$(DSPLBASE)/FastMathFunctions		\
	$(DSPLBASE)/FilteringFunctions		\
	$(DSPLBASE)/MatrixFunctions		\
	$(DSPLBASE)/StatisticsFunctions		\
	$(DSPLBASE)/SupportFunctions		\
	$(DSPLBASE)/TransformFunctions


# Sources for building dsplib

libcmsis_dsp-objs-y := $(foreach dir,$(DSPLDIRS),$(wildcard $(dir)/*.c $(dir)/*.S))

# The below line is added to remove sdk/external/cmsis
# from file paths.
libcmsis_dsp-objs-y := $(libcmsis_dsp-objs-y:$(d)/%=%)


