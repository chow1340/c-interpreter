CC = gcc
CFLAGS = -c -g

OBJ = main

all: $(OBJ)

$(OBJ): main.o vm.o debug.o chunk.o scanner.o value.o memory.o compiler.o object.o table.o

vm.o: vm.c 
	$(CC) $(CFLAGS) vm.c 
main.o: main.c
	$(CC) $(CFLAGS) main.c
debug.o: debug.c
	$(CC) $(CFLAGS) debug.c
chunk.o: chunk.c
	$(CC) $(CFLAGS) chunk.c
scanner.o: scanner.c
	$(CC) $(CFLAGS) scanner.c
value.o: value.c
	$(CC) $(CFLAGS) value.c
memory.o: memory.c
	$(CC) $(CFLAGS) memory.c
compiler.o: compiler.c
	$(CC) $(CFLAGS) compiler.c
object.o: object.c
	$(CC) $(CFLAGS) object.c
table.o: table.c
	$(CC) $(CFLAGS) table.c

exec:
	./main

clean:
	rm -f *.o