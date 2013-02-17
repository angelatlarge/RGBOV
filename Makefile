
.PHONY: all TLC5940 74HC595 clean

all: TLC5490 74HC595

74HC595:
	echo Building 74HC595
	@$(MAKE) -C 74HC595

TLC5940:
	echo Building TLC5940
	@$(MAKE) -C TLC5940

clean:
	@$(MAKE) -C TLC5940 clean
	@$(MAKE) -C 74HC595 clean
