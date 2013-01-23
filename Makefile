
#########  AVR Project Makefile Template   #########
######                                        ######
######    Copyright (C) 2003-2005,Pat Deegan, ######
######            Psychogenic Inc             ######
######          All Rights Reserved           ######
######                                        ######
###### You are free to use this code as part  ######
###### of your own applications provided      ######
###### you keep this copyright notice intact  ######
###### and acknowledge its authorship with    ######
###### the words:                             ######
######                                        ######
###### "Contains software by Pat Deegan of    ######
###### Psychogenic Inc (www.psychogenic.com)" ######
######                                        ######
###### If you use it as part of a web site    ######
###### please include a link to our site,     ######
###### http://electrons.psychogenic.com  or   ######
###### http://www.psychogenic.com             ######
######                                        ######
####################################################


##### This Makefile will make compiling Atmel AVR 
##### micro controller projects simple with Linux 
##### or other Unix workstations and the AVR-GCC 
##### tools.
#####
##### It supports C, C++ and Assembly source files.
#####
##### Customize the values as indicated below and :
##### make
##### make disasm 
##### make stats 
##### make hex
##### make writeflash
##### make gdbinit
##### or make clean
#####
##### See the http://electrons.psychogenic.com/ 
##### website for detailed instructions


####################################################
#####                                          #####
#####              Configuration               #####
#####                                          #####
##### Customize the values in this section for #####
##### your project. MCU, PROJECTNAME and       #####
##### PRJSRC must be setup for all projects,   #####
##### the remaining variables are only         #####
##### relevant to those needing additional     #####
##### include dirs or libraries and those      #####
##### who wish to use the avrdude programmer   #####
#####                                          #####
##### See http://electrons.psychogenic.com/    #####
##### for further details.                     #####
#####                                          #####
####################################################


#####         Target Specific Details          #####
#####     Customize these for your project     #####

# Name of target controller 
# (e.g. 'at90s8515', see the available avr-gcc mmcu 
# options for possible values)

#~ MCU=attiny2313
#~ MCU=atmega48a
#~ MCU=atmega16
#~ MCU=atmega32
MCU=atmega328p
MACROS+=-D F_CPU=8000000UL

#~ # For atmega644
#~ MCU=atmega644a
#~ PROGRAMMER_MCU=m644
#~ MCU_SIZE=atmega644
#~ MACROS+=-D SPEAKERPIN_m644_OC2A
#~ MACROS+=-D F_CPU=8000000UL

# For ATmega 32
#~ MCU=atmega32
#~ MACROS+=-D SPEAKERPIN_m32_OC2
#~ MACROS+=-D F_CPU=4000000UL


# Macro definitions
# Tried 4, 8, 1
MACROS+=-D BAUD=9600
MACROS+=-D kBEEP_CONTROL_MAXENTRIES=32
#~ MACROS+=-D BAUD=19200
#~ MACROS+=-D BAUD=9600
#~ MACROS+=-D BAUD=2400

# id to use with programmer
# default: PROGRAMMER_MCU=$(MCU)
# In case the programer used, e.g avrdude, doesn't
# accept the same MCU name as avr-gcc (for example
# for ATmega8s, avr-gcc expects 'atmega8' and 
# avrdude requires 'm8')
ifndef PROGRAMMER_MCU
PROGRAMMER_MCU=$(MCU)
endif

# avr-nm does not know as much about the chips
ifndef MCU_SIZE
MCU_SIZE=$(MCU)
endif


# Name of our project
# (use a single word, e.g. 'myproject')
PROJECTNAME=RGBOV

# Source files
# List C/C++/Assembly source files:
# (list all files to compile, e.g. 'a.c b.cpp as.S'):
# Use .cc, .cpp or .C suffix for C++ files, use .S 
# (NOT .s !!!) for assembly source code files.
VPATH += :../avr_libs/sr595/
VPATH += :../avr_libs/serial/
VPATH += :../avr_libs/strfmt/
	
PRJSRC+=RGBOV.cpp
PRJSRC+=../avr_libs/sr595/sr595.cpp
PRJSRC+=../avr_libs/serial/serial.c
PRJSRC+=../avr_libs/serial/strfmt.cpp

# additional includes (e.g. -I/path/to/mydir)
#~ INC+=-I../avr_libs/Arduino
INC+=-I../avr_libs/sr595 
INC+=-I../avr_libs/serial
INC+=-I../avr_libs/strfmt

# libraries to link in (e.g. -lmylib)
LIBDIRS+=-L../avr_libs/sr595 
LIBDIRS+=-L../avr_libs/serial
LIBDIRS+=-L../avr_libs/strfmt

#libs
LIBS+=$(AVR_DIR)/avr/lib/libm.a

# Optimization level, 
# use s (size opt), 1, 2, 3 or 0 (off)
OPTLEVEL=s


#####      AVR Dude 'writeflash' options       #####
#####  If you are using the avrdude program
#####  (http://www.bsdhome.com/avrdude/) to write
#####  to the MCU, you can set the following config
#####  options and use 'make writeflash' to program
#####  the device.


# programmer id--check the avrdude for complete list
# of available opts.  These should include stk500,
# avr910, avrisp, bsd, pony and more.  Set this to
# one of the valid "-c PROGRAMMER-ID" values 
# described in the avrdude info page.
# 
AVRDUDE_PROGRAMMER= -c usbtiny

# port--serial or parallel port to which your 
# hardware programmer is attached
#
AVRDUDE_PORT=/dev/ttyS1

# Where the files get assembled to
OBJDIR=obj

####################################################
#####                Config Done               #####
#####                                          #####
##### You shouldn't need to edit anything      #####
##### below to use the makefile but may wish   #####
##### to override a few of the flags           #####
##### nonetheless                              #####
#####                                          #####
####################################################


##### Flags ####

# HEXFORMAT -- format for .hex file output
HEXFORMAT=ihex

# compiler
CONLYFLAGS= \
	-Wstrict-prototypes						\
	-Wa,-ahlms=$(OBJDIR)/$(notdir $(firstword $(filter %.lst, $(<:.c=.lst))))

CALLFLAGS=\
	-I. $(INC) -g -mmcu=$(MCU) -O$(OPTLEVEL) \
	-fpack-struct 							\
	-fshort-enums             				\
	-funsigned-bitfields 					\
	-funsigned-char    						\
	$(MACROS)								\
	-Wall					                \
											\
    -ffunction-sections 					\
    -fdata-sections 						
	
# c++ specific flags
CPPFLAGS=	\
	-fno-exceptions               \
	-Wa,-ahlms=$(OBJDIR)/$(notdir $(firstword $(filter %.lst, $(<:.cpp=.lst)) $(filter %.lst, $(<:.cc=.lst)) $(filter %.lst, $(<:.C=.lst))))	\
	-Wno-uninitialized

# assembler
ASMFLAGS =-I. $(INC) -mmcu=$(MCU)        \
	-x assembler-with-cpp            \
	-Wa,-gstabs,-ahlms=$(firstword $(<:.S=.lst) $(<.s=.lst))


# linker
MAPFILE=$(TRG).map
LDFLAGS=									\
		-Wl,--gc-sections,-Map,$(MAPFILE) \
		-mmcu=$(MCU) 						\
		$(LIBDIRS) 							\
		$(LIBS)

# Would have liked to put --cref,--relax in there, but they coredump gcc :(
		#~ -Wl,--gc-sections,-Map,$(MAPFILE),--cref,--relax \
		
# Old linker flags:
#~ LDFLAGS=-Wl,-Map,$(MAPFILE) -mmcu=$(MCU) \
	#~ --gc-sections			\
	#~ $(LIBDIRS) $(LIBS)

##### directories ####
AVR_DIR=/usr/local/avr
TOOLS_DIR=$(AVR_DIR)/bin

##### executables ####
CC=$(TOOLS_DIR)/avr-gcc
OBJCOPY=$(TOOLS_DIR)/avr-objcopy
SYMLIST=$(TOOLS_DIR)/avr-nm
OBJDUMP=$(TOOLS_DIR)/avr-objdump
#~ SIZE=$(TOOLS_DIR)/avr-size -B
SIZE=/home/kirill/arduino/arduino-1.0/hardware/tools/avr/bin/avr-size 
SIZEOPTS+=-C --mcu=${MCU_SIZE}
AVRDUDE=/home/kirill/arduino/arduino-1.0/hardware/tools/avr/bin/avrdude.exe
REMOVE=rm -f

##### fuse files ####
ifndef ISP_READFUSEFILE_LOW_FILE
ISP_READFUSEFILE_LOW_FILE = readFuseValue_low.bin
endif

ifndef ISP_READFUSEFILE_HIGH_FILE
ISP_READFUSEFILE_HIGH_FILE = readFuseValue_high.bin
endif

ifndef ISP_READFUSEFILE_EXTRA_FILE
ISP_READFUSEFILE_EXTRA_FILE = readFuseValue_extra.bin
endif

ISP_FULE_FILES=$(ISP_READFUSEFILE_LOW_FILE) $(ISP_READFUSEFILE_HIGH_FILE) $(ISP_READFUSEFILE_EXTRA_FILE)

# Display fuse values command
ifndef SHOW_FUSE_VALUES
SHOW_FUSE_VALUES= @echo \
	' Low: 0x'`od $(ISP_READFUSEFILE_LOW_FILE) -tx1 | head -1 | sed -e 's/0000000 *//'` \
	' High: 0x'`od $(ISP_READFUSEFILE_HIGH_FILE) -tx1 | head -1 | sed -e 's/0000000 *//'` \
	' Extra: 0x'`od $(ISP_READFUSEFILE_EXTRA_FILE) -tx1 | head -1 | sed -e 's/0000000 *//'`
endif 


##### avrdude options ####
AVRDUDE_CONF = C:\\cygwin\\home\\kirill\\arduino\\arduino-1.0\\hardware\\tools\\avr\\etc\\avrdude.conf
#~ ifndef MCU_DUDE
#~ MCU_DUDE=$(MCU)
#~ endif
AVRDUDE_COM_OPTS = 			\
	-p $(PROGRAMMER_MCU) 	
	#~ -D						\
	#~ -V
ifdef AVRDUDE_CONF
AVRDUDE_COM_OPTS += -C $(AVRDUDE_CONF)
endif
ifndef AVRDUDE_PROGRAMMER
AVRDUDE_PROGRAMMER	   = -c stk500v2
endif
ifdef ISP_PORT
ISP_POST_OPTION = -P $(ISP_PORT)
endif 
AVRDUDE_ISP_OPTS = $(ISP_POST_OPTION) $(AVRDUDE_PROGRAMMER)

##### automatic target names ####
TRG=$(OBJDIR)/$(PROJECTNAME).elf
DUMPTRG=$(OBJDIR)/$(PROJECTNAME).s

HEXROMTRG=$(OBJDIR)/$(PROJECTNAME).hex 
SYMBOLLIST_A=$(OBJDIR)/$(PROJECTNAME).sym_a
SYMBOLLIST_B_RAW=$(OBJDIR)/$(PROJECTNAME).sym_b
SYMBOLLIST_B_SORTSIZE=$(OBJDIR)/$(PROJECTNAME).sym_b_sizesort
HEXTRG=$(HEXROMTRG) $(OBJDIR)/$(PROJECTNAME).ee.hex
GDBINITFILE=$(OBJDIR)/gdbinit-$(PROJECTNAME)

# Define all object files.

# Start by splitting source files by type
#  C++
CPPFILES=$(filter %.cpp, $(PRJSRC))
CCFILES=$(filter %.cc, $(PRJSRC))
BIGCFILES=$(filter %.C, $(PRJSRC))
#  C
CFILES=$(filter %.c, $(PRJSRC))
#  Assembly
ASMFILES=$(filter %.S, $(PRJSRC))

#	objects = 			$(patsubst %.cpp,obj/%.o,$(src)) 
#	test_objects = 		$(patsubst %.cpp,obj/%.o,$(test_src)) 
# List all object files we need to create
OBJDEPS=										\
	$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(CFILES))) 			\
	$(patsubst %.cpp,$(OBJDIR)/%.o,$(notdir $(CPPFILES))) 		\
	$(patsubst %.C,$(OBJDIR)/%.o,$(notdir $(BIGCFILES))) 		\
	$(patsubst %.cc,$(OBJDIR)/%.o,$(notdir $(CCFILES))) 		\
	$(patsubst %.S,$(OBJDIR)/%.o,$(notdir $(ASMFILES))) 
#~ OBJDEPS=\
	#~ $(CFILES:.c=.o)    \
	#~ $(CPPFILES:.cpp=.o)\
	#~ $(BIGCFILES:.C=.o) \
	#~ $(CCFILES:.cc=.o)  \
	#~ $(ASMFILES:.S=.o)

# Define all lst files.
LST=$(filter %.lst, $(OBJDEPS:.o=.lst))

# All the possible generated assembly 
# files (.s files)
GENASMFILES=$(filter %.s, $(OBJDEPS:.o=.s)) 


.SUFFIXES : .c .cc .cpp .C .o .elf .s .S \
	.hex .ee.hex .h .hh .hpp


.PHONY: writeflash clean stats gdbinit stats

# Make targets:
# all, disasm, stats, hex, writeflash/install, clean
all: $(TRG) symbols

disasm: $(DUMPTRG) stats

stats: $(TRG)
	$(OBJDUMP) -h $(TRG)
	$(SIZE) $(SIZEOPTS) $(TRG) 
	
hex: $(HEXTRG)

symbols: $(SYMBOLLIST_A) $(SYMBOLLIST_B_RAW)  

writeflash: hex
	$(AVRDUDE) 				\
		$(AVRDUDE_COM_OPTS) \
		$(AVRDUDE_ISP_OPTS) \
		-U flash:w:$(HEXROMTRG)

install: writeflash

ispload: writeflash

readfuse: readfuses

readfuses: isp_readfuse

isp_readfuse: isp_readfuses

isp_readfuses:
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
			-U lfuse:r:$(ISP_READFUSEFILE_LOW_FILE):r \
			-U hfuse:r:$(ISP_READFUSEFILE_HIGH_FILE):r \
			-U efuse:r:$(ISP_READFUSEFILE_EXTRA_FILE):r
		$(SHOW_FUSE_VALUES)
		$(REMOVE) $(ISP_FULE_FILES)


$(DUMPTRG): $(TRG) 
	$(OBJDUMP) -S  $< > $@


$(TRG): $(OBJDEPS) 
	@echo ''
	@echo @@@ Making $(TRG) from $(OBJDEPS) @@@
	$(CC) $(LDFLAGS) -o $(TRG) $(OBJDEPS) -lm
#	$(CC) $(LDFLAGS) -o $(TRG) $(OBJDEPS)
	$(SIZE) $(SIZEOPTS) $(TRG) 

#	Other makefile
#	$(CC) $(LDFLAGS) -o $@ $(PROJECT).o $(EXTRA_OBJS) -lc -lm


#### Generating assembly ####
# asm from C
%.s: %.c
	$(CC) -S $(CALLFLAGS) $(CONLYFLAGS) $< -o $@

# asm from (hand coded) asm
%.s: %.S
	$(CC) -S $(ASMFLAGS) $< > $@


# asm from C++
.cpp.s .cc.s .C.s :
	$(CC) -S $(CALLFLAGS) $(CPPFLAGS) $< -o $@



#### Generating object files ####
# object from C
#~ .c.o: 
$(OBJDIR)/%.o : %.c
	@echo ''
	@echo @@@ C: Building $< from $@ @@@
	$(CC) $(CALLFLAGS) $(CONLYFLAGS) -c $< -o $@


# object from C++ (.cc, .cpp, .C files)
#~ .cc.o .cpp.o .C.o :
$(OBJDIR)/%.o : %.cpp
	@echo ''
	@echo @@@ C++: Building $@ from $< @@@
	$(CC) $(CALLFLAGS) $(CPPFLAGS) -c $< -o $@

# object from asm
.S.o :
	$(CC) $(ASMFLAGS) -c $< -o $@


# Generating a symbol listing file
$(OBJDIR)/%.sym_a : $(OBJDIR)/%.elf
	$(SYMLIST) --print-size --size-sort -C $< > $@

$(OBJDIR)/%.sym_b :  $(OBJDIR)/%.elf
	$(OBJDUMP) -t -j .bss -j .data $< > $@

$(OBJDIR)/%.sym_b_sizesort :  $(OBJDIR)/%.sym_b
	cat $< | awk '{print $$4 $$5 $$6}' > $@

SYMBOLLIST_B_RAW=$(OBJDIR)/$(PROJECTNAME).sym_b
SYMBOLLIST_B_SORTSIZE=$(OBJDIR)/$(PROJECTNAME).


#~ .elf.syma :
	#~ $(SYMLIST) -C $< > $@

#~ .elf.symb :
	#~ $(OBJDUMP) -t -j .bss $< > $@

#### Generating hex files ####
# hex files from elf
#####  Generating a gdb initialisation file    #####
.elf.hex:
	$(OBJCOPY) -j .text                    \
		-j .data                       \
		-O $(HEXFORMAT) $< $@

.elf.ee.hex:
	$(OBJCOPY) -j .eeprom                  \
		--change-section-lma .eeprom=0 \
		-O $(HEXFORMAT) $< $@


#####  Generating a gdb initialisation file    #####
##### Use by launching simulavr and avr-gdb:   #####
#####   avr-gdb -x gdbinit-myproject           #####
gdbinit: $(GDBINITFILE)

$(GDBINITFILE): $(TRG)
	@echo "file $(TRG)" > $(GDBINITFILE)
	
	@echo "target remote localhost:1212" \
		                >> $(GDBINITFILE)
	
	@echo "load"        >> $(GDBINITFILE) 
	@echo "break main"  >> $(GDBINITFILE)
	@echo "continue"    >> $(GDBINITFILE)
	@echo
	@echo "Use 'avr-gdb -x $(GDBINITFILE)'"


#### Cleanup ####
clean:
	$(REMOVE) $(TRG) $(MAPFILE) $(DUMPTRG)
	$(REMOVE) $(OBJDEPS)
	$(REMOVE) $(LST) $(GDBINITFILE)
	$(REMOVE) $(GENASMFILES)
	$(REMOVE) $(HEXTRG)
	


#####                    EOF                   #####

