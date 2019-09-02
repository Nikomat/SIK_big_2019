CC = gcc
STD = c11
CFLAGS = -Wall -Wextra -O2
TARGETS = netstore-client netstore-server

all: $(TARGETS)

err.o: utils/err.c utils/err.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/err.c -o err.o

args_utils.o: utils/args_utils.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/args_utils.c -o args_utils.o

command_utils.o: utils/command_utils.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/command_utils.c -o command_utils.o

connection_utils.o: utils/connection_utils.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/connection_utils.c -o connection_utils.o

file_utils.o: utils/file_utils.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/file_utils.c -o file_utils.o

user_input_output.o: utils/user_input_output.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) utils/user_input_output.c -o user_input_output.o



server.o: server/main.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) server/main.c -o server.o

client.o: client/main.c utils/args_utils.h utils/command_utils.h utils/connection_utils.h utils/err.h utils/file_utils.h utils/types.h utils/user_input_output.h
	$(CC) -std=$(STD) -c $(LFLAGS) client/main.c -o client.o


netstore-client: client.o err.o args_utils.o command_utils.o connection_utils.o file_utils.o user_input_output.o
	$(CC) -std=$(STD) $(LFLAGS) -o netstore-client client.o err.o args_utils.o command_utils.o connection_utils.o file_utils.o user_input_output.o
netstore-server: server.o err.o args_utils.o command_utils.o connection_utils.o file_utils.o user_input_output.o
	$(CC) -std=$(STD) $(LFLAGS) -o netstore-server server.o err.o args_utils.o command_utils.o connection_utils.o file_utils.o user_input_output.o

clean:
	rm -f *.o $(TARGETS)
