
t_uartboot := $(d)/uartboot$(file_ext)

$(t_uartboot):
	@echo "[$(notdir $@)]"
	$(AT)$(MAKE) -s -C $(dir $@) CC=$(HOST_CC) TARGET=$(notdir $@)

$(t_uartboot).clean:
	@echo " [clean] $(notdir $(t_uartboot))"
	$(AT)$(MAKE) -s -C $(dir $@) CC=$(HOST_CC) TARGET=$(notdir $(t_uartboot)) clean

tools.install: $(t_uartboot)
tools.clean : $(t_uartboot).clean
