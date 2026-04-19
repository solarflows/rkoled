# C Config
INCLUDE_PATHS   := ./inc
SOURCE_PATHS    := ./src
OBJECT_DIR      := ./obj
LIBS_NAME       := pthread
TARGET_FILE     := ./RTDevInfo
CFLAGS       	:= -c

CXX_FILES       := $(shell find $(SOURCE_PATHS)/*.c)
HXX_FILES		:= $(shell find $(INCLUDE_PATHS)/*.h)
OBJ_FILES       := $(patsubst $(SOURCE_PATHS)/%.c,$(OBJECT_DIR)/%.o,$(CXX_FILES))
LIBS            := $(LIBS_NAME:%=-l%)
INCLUDES        := $(INCLUDE_PATHS:%=-I%)

CC      ?= gcc
CFLAGS  ?= -O2
LDFLAGS ?=
INCLUDES := -I./inc
LIBS     := -lpthread

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, obj/%.o, $(SRC))
TARGET = RTDevInfo

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

default: $(TARGET)

clean:
	rm -rf obj *.o $(TARGET)

.PHONY: default clean

$(OBJECT_DIR)/%.o : $(SOURCE_PATHS)/%.c $(HXX_FILES)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(INCLUDES) -o $@

compile : $(TARGET_FILE)

$(TARGET_FILE) : $(OBJ_FILES)
	$(CC) -o $@ $(OBJ_FILES) $(LIBS)

build: $(TARGET_FILE)

clean :
	@rm -rf $(OBJECT_DIR)
	@rm -f $(TARGET_FILE)

run : $(TARGET_FILE)
	$(TARGET_FILE)

init :
	mkdir inc
	mkdir src
	touch src/main.c

.PHONY : build compile run clean init
