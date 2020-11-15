CC = gcc
LIB?= ./lib/
FILE_SERVER = $(LIB)utils.c $(LIB)gbn/sender.c $(LIB)gbn/receiver.c server.c -lm -lpthread -lrt
FILE_CLIENT = $(LIB)utils.c $(LIB)gbn/sender.c $(LIB)gbn/receiver.c client.c -lm -lpthread -lrt

do:
	$(CC) $(FILE_SERVER) -o ./out/server.out
	$(CC) $(FILE_CLIENT) -o ./out/client.out

	@echo " "
	@echo "Compilato"
clean:
	rm ./out/server.out
	rm ./out/client.out

	@echo " "
	@echo "File eliminati!"
