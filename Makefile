SHELLOBJS	= shell.o fat.o disksim.o fat_shell.o entrylist.o clusterlist.o

all: $(SHELLOBJS)
	$(CC) -o shell $(SHELLOBJS) -Wall

clean:
	rm *.o
	rm shell
