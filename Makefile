CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=gnu99
INCLUDES := -I./inc
TARGET := rkoled
COMPAT_TARGET := RTDevInfo

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

all: $(TARGET) $(COMPAT_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(COMPAT_TARGET): $(TARGET)
	@ln -sf $(TARGET) $@ 2>/dev/null || cp -f $(TARGET) $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) $(COMPAT_TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)/usr/bin/$(COMPAT_TARGET)

.PHONY: all clean install
