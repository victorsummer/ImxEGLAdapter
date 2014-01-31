cc=gcc
#CFLAGS=-g -c -Wall -m64 -Ofast -flto -march=native -funroll-loops -DLINUX
CFLAGS=-g -c -Wall -mthumb -O3 -march=armv7-a -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon -mvectorize-with-neon-quad -mfloat-abi=hard -DLINUX -DEGL_API_FB -DGL_UNPACK_ROW_LENGTH=0x0CF2
LDFLAGS=-lEGL -lGLESv2 -lX11 -lpthread
SRCS=main.c
OBJS=$(SRCS:.c=.o)
TARGET=project-12

all: $(SRCS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -fr $(OBJS) $(TARGET)
