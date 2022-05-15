XENO_SKIN = posix
MAIN_SRC=Realtime_EtherCAT_test
TARGET=Realtime_EtherCAT_test

SOEM_INC_DIR = /usr/local/include/soem
SOEM_LIB_DIR = /usr/local/lib
SOEM_LIBS = -lsoem

ALCHEMY_INC_DIR = /usr/xenomai/include/alchemy
ALCHEMY_LIBS = -lalchemy -lcopperplate

CFLAGS = -I$(SOEM_INC_DIR) -I$(ALCHEMY_INC_DIR) $(shell xeno-config --skin=$(XENO_SKIN) --cflags)
LDFLAGS = -L$(SOEM_LIB_DIR) $(SOEM_LIBS) $(ALCHEMY_LIBS) $(shell xeno-config --skin=$(XENO_SKIN) --ldflags)
CC := $(shell xeno-config --cc)


SRC_TARGET = $(TARGET:=.c)
OBJ_TARGET = $(SRC_TARGET:.c=.o)
OBJS = $(OBJ_TARGET)
$(TARGET): $(MAIN_SRC).c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) 
