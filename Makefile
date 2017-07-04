CC = gcc 

memsim:
	$(CC) memsim.c -o memsim

clean:
	rm memsim

