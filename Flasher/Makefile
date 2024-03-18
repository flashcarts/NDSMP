.PHONY: all

DIRS := ndsmp_nds ndsmp_gba

all:
	$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR);)

clean:
	$(foreach DIR, $(DIRS), $(MAKE) -C $(DIR) clean;)
