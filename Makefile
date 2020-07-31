CC = gcc
FLAGS = -c

OBJ = main

all: $(OBJ)

$(OBJ): main.o vm.o debug.o chunk.o scanner.o value.o memory.o 

vm.o: vm.c 
	$(CC) $(FLAGS) vm.c 
main.o: main.c
	$(CC) $(FLAGS) main.c
debug.o: debug.c
	$(CC) $(FLAGS) debug.c
chunk.o: chunk.c
	$(CC) $(FLAGS) chunk.c
scanner.o: scanner.c
	$(CC) $(FLAGS) scanner.c
value.o: value.c
	$(CC) $(FLAGS) value.c
memory.o: memory.c
	$(CC) $(FLAGS) memory.c

exec:
	./main

clean:
	rm -f *.o