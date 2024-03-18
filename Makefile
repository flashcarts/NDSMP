.PHONY: all

LOADER_DIR := BootLoader
FLASHER_DIR := Flasher
DIRS := $(LOADER_DIR) $(FLASHER_DIR)

all:
	$(MAKE) -C $(LOADER_DIR)
	@cp $(LOADER_DIR)/loader.bin $(FLASHER_DIR)/data/loader.bin
	$(MAKE) -C $(FLASHER_DIR)
	@cp $(FLASHER_DIR)/ndsmp_nds/ndsmp.nds ./ndsmp.nds
	@cp $(FLASHER_DIR)/ndsmp_gba/ndsmp_mb.gba ./ndsmp.gba
	@touch ./ndsmp.txt
	
clean:
	$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR) clean;)
