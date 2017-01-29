PROGRAM  = capmeter
CROSS    = arm-none-eabi-
COMPORT  = /dev/ttyUSB0
OBJS     = main.o \
           acmp.o \
           uart.o \
           startup.o \


###############################################################################

# .small-multiply does not necessarily reduce size.
# -flto -fno-builtin -fno-common not used.
# -fpack-struct -fshort-enums not used.
CPU      = -mthumb -mcpu=cortex-m0plus
FPU      = -mfloat-abi=soft
LDSCRIPT = lpc810.ld
MAPFILE  = $(PROGRAM).map

CC       = $(CROSS)gcc
LD       = $(CROSS)ld
OBJCOPY  = $(CROSS)objcopy
SIZE     = $(CROSS)size

CFLAGS   = $(CPU) $(FPU)
# Newer GCC can afford this (NOTE: -O3 breaks code somehow)
CFLAGS  += -O2 -finline-functions
# Otherwise use this to keep code size within 4K
#CFLAGS  += -Os
CFLAGS  += --specs=nano.specs -Wall -ffunction-sections -fdata-sections
CFLAGS  += -fsingle-precision-constant -funsigned-char -funsigned-bitfields
LDFLAGS  = -T$(LDSCRIPT) -Map $(MAPFILE) --gc-section

INC     += -Icmsis
CFLAGS  += $(INC) -D__USE_CMSIS -DCLOCK_SETUP=1
OBJS    += cmsis/system_lpc8xx.o

# It is likely libc is needed if -O3 is used
LIBM     = $(shell $(CC) $(CFLAGS) --print-file-name=libm.a)
LIBC     = $(shell $(CC) $(CFLAGS) --print-file-name=libc.a)
LIBGCC   = $(shell $(CC) $(CFLAGS) --print-libgcc-file-name)

all: $(PROGRAM).bin $(PROGRAM).hex size

$(PROGRAM).elf: $(LDSCRIPT) $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $(OBJS) $(LIBGCC)

%.hex: %.elf
	$(OBJCOPY) -S -O ihex   $< $@

%.bin: %.elf
	$(OBJCOPY) -S -O binary $< $@


.PHONY: clean size flash

size: $(PROGRAM).elf
	@echo ""
	@$(SIZE) $(PROGRAM).elf
	@echo ""

flash: $(PROGRAM).hex $(COMPORT)
	lpc21isp -verify $(PROGRAM).hex $(COMPORT) 9600 30000000

clean:
	rm -f $(OBJS) $(PROGRAM).elf $(PROGRAM).hex $(PROGRAM).bin $(PROGRAM).map
