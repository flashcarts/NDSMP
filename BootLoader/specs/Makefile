export PATH		:=	$(DEVKITARM)/bin:$(PATH)

PREFIX			:=	arm-elf-
export CC		:=	$(PREFIX)gcc
export CXX		:=	$(PREFIX)g++
export AR		:=	$(PREFIX)ar
export OBJCOPY	:=	$(PREFIX)objcopy

all:
	@echo -n Compiling gba_mp_ds_crt0...
	@$(CC)  -x assembler-with-cpp -marm -mthumb-interwork -c gba_mp_ds_crt0.s -o gba_mp_ds_crt0.o
	@echo done.
	
	@echo -n Rewriting specs file...
	@echo '*startfile:' > gba_mp_ds.specs
	@echo -n $(shell pwd -W) >> gba_mp_ds.specs
	@echo '/../specs/gba_mp_ds_crt0%O%s crti%O%s crtbegin%O%s' >> gba_mp_ds.specs
	@echo done.
	
clean:
	@echo Clean...
	rm -f gba_mp_ds.specs gba_mp_ds_crt0.o
	@echo done.