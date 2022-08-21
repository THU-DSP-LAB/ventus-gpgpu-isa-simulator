CC = riscv64-unknown-elf-gcc
CFLAG_C = -fno-common -fno-builtin-printf -specs=htif_nano.specs -Wl,--defsym=__main=main -c 
CFLAG_LD = -static -specs=htif_nano.specs -Wl,--defsym=__main=main

TEST ?= branch-basic
SRC = $(TEST).s
OBJ = build/$(TEST).o
TARGET = build/$(TEST).riscv

P ?= 4
VARCH ?= vlen:256,elen:32
GPGPUARCH ?= numw:4,numt:8
DEBUG ?= -d
ISA ?= rv64gv
SPIKE_ARGS = $(DEBUG) -p$(P) --isa $(ISA) --varch $(VARCH) --gpgpuarch $(GPGPUARCH)

all: $(TARGET)

spike-sim: $(TARGET)
	spike $(SPIKE_ARGS) $(TARGET)

$(OBJ): $(SRC)
	$(CC) $(CFLAG_C) $(SRC) -o $(OBJ)

$(TARGET): $(OBJ)
	$(CC) $(CFLAG_LD) $(OBJ) -o $(TARGET)

clean:
	rm build/*
