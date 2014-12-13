PRG      = tool
OBJ      = tool.o
OPTIMIZE = -O2

CC       = gcc
CFLAGS   = -g -Wall $(OPTIMIZE)
LIBS     = `pkg-config --libs --cflags libusb-1.0`

.PHONY: all clean

all: $(PRG)

$(PRG): $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) -o $(PRG) $(PRG).o

.c.o:
	$(CC) $(CFLAGS) $(LIBS) $(LDFLAGS) -c $< -o $@

clean:
	rm -f *.o $(PRG)

