CC = riscv64-unknown-elf-gcc
CFLAG_C = -fno-common -fno-builtin-printf -specs=htif_nano.specs -c 
CFLAG_LD = -static -specs=htif_nano.specs

TEST ?= branch-basic
SRC = $(TEST).s
OBJ = $(TEST).o
TARGET = $(TEST).riscv

P ?= 1
VARCH ?= vlen:256,elen:32
DEBUG ?= -d
ISA ?= rv64gv
SPIKE_ARGS = $(DEBUG) -p$(P) --isa $(ISA) --varch $(VARCH)

all: $(TARGET)

spike-sim: $(TARGET)
	spike $(SPIKE_ARGS) $(TARGET)

$(OBJ): $(SRC)
	$(CC) $(CFLAG_C) $(SRC)

$(TARGET): $(OBJ)
	$(CC) $(CFLAG_LD) $(OBJ) -o $(TARGET)

clean:
	rm *.riscv *.o
