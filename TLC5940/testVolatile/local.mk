HOST := $(shell hostname)
ifeq ($(HOST),pajal)
##################################
##### CONFIGURATION FOR pajal ####
##################################

# directories 
AVR_DIR=/usr/local/avr
LIBC_DIR=/usr/local/avr/avr/lib
TOOLS_DIR=$(AVR_DIR)/bin
# executables 
CC=$(TOOLS_DIR)/avr-gcc
#~ CC=/usr/local/avr/bin/avr-gcc-4.7.3.exe
#~ CC=/home/kirill/gcc/gcc-4.7.3/build/gcc/xgcc.exe
#~ CC=/home/kirill/gcc/gcc-4.7.3/host-i686-pc-cygwin/gcc/xgcc.exe
OBJCOPY=$(TOOLS_DIR)/avr-objcopy
OBJDUMP=$(TOOLS_DIR)/avr-objdump
SYMLIST=$(TOOLS_DIR)/avr-nm
#~ SIZE=$(TOOLS_DIR)/avr-size
SIZE=/home/kirill/arduino/arduino-1.0/hardware/tools/avr/bin/avr-size 
AVRDUDE=/home/kirill/arduino/arduino-1.0/hardware/tools/avr/bin/avrdude.exe
REMOVE=rm -f

##### avrdude conf file ####
AVRDUDE_CONF = C:\\cygwin\\home\\kirill\\arduino\\arduino-1.0\\hardware\\tools\\avr\\etc\\avrdude.conf

##### serial port for uploading ####
AVRDUDE_PORT_SERIAL = COM18

else ifeq ($(HOST),jetpack)
##################################
##### CONFIGURATION FOR jetpack ####
##################################


# directories 
AVR_DIR=/usr/bin
LIBC_DIR=/usr/lib/avr/lib
TOOLS_DIR=$(AVR_DIR)
# executables 
CC=$(TOOLS_DIR)/avr-gcc
OBJCOPY=$(TOOLS_DIR)/avr-objcopy
SYMLIST=$(TOOLS_DIR)/avr-nm
OBJDUMP=$(TOOLS_DIR)/avr-objdump
SIZE=$(TOOLS_DIR)/avr-size
AVRDUDE=$(TOOLS_DIR)/avrdude.exe
REMOVE=rm -f

##### avrdude conf file ####
# not defined

##### serial port for uploading ####
#~ AVRDUDE_PORT_SERIAL = COM18

endif