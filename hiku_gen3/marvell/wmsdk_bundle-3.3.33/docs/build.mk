
.PHONY:  docs docs.clean

docs:
	$(AT)$(MAKE) -s -C docs/ docs.all

docs.clean:
	$(AT)$(MAKE) -s -C docs/ clean

