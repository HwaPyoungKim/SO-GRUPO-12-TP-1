CC = gcc
CFLAGS = -Wall
VIEWFLAGS = -lpthread -lrt -lncurses
MASTERPLAYERFLAGS = -lm

SRCS = master.c player.c vista.c utils.c
OBJS = $(SRCS:.c=.o)

TARGETS = master player1 player2 player3 player4 player5 player6 player7 player8 player9 vista

all: $(TARGETS)

master: master.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player1: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player2: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player3: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player4: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player5: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player6: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player7: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player8: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

player9: player.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(MASTERPLAYERFLAGS)

vista: vista.o
	$(CC) $(CFLAGS) -o $@ $^ $(VIEWFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)