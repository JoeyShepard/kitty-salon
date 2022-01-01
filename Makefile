TARGET=kitty-salon
OBJECTS=main.o wav-data.o
MAP=main.map
MAKEFILE=Makefile

RM= rm -rf

GCC_DIR = $(abspath $(dir $(lastword $(MAKEFILE)))/../../msp430-gcc/bin)
SUPPORT_FILE_DIRECTORY = $(abspath $(dir $(lastword $(MAKEFILE)))/../../msp430-gcc/include)

DEVICE  = MSP430G2553
CC      = $(GCC_DIR)/msp430-elf-gcc
GDB     = $(GCC_DIR)/msp430-elf-gdb
OBJCOPY = $(GCC_DIR)/msp430-elf-objcopy

CFLAGS = -I $(SUPPORT_FILE_DIRECTORY) -mmcu=$(DEVICE) -Og -Wall -g
LFLAGS = -L $(SUPPORT_FILE_DIRECTORY) -Wl,-Map,$(MAP),--gc-sections 

all: $(TARGET)
	$(OBJCOPY) -O ihex $(TARGET).out $(TARGET).hex
    
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $? -o $(TARGET).out

clean: 
	$(RM) $(OBJECTS)
	$(RM) $(MAP)
	$(RM) *.out
	
debug: all
	$(GDB) $(DEVICE).out
