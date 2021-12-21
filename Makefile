EXEC = libzbus
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

CFLAGS += -g -W -Wall
LDFLAGS += -lhiredis -lmsgpackc -luuid

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) *.o

mrproper: clean
	$(RM) $(EXEC)
