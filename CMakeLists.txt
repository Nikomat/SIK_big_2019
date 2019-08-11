cmake_minimum_required(VERSION 3.13)
project(server C)
project(client C)

set(CMAKE_C_STANDARD 11)

add_executable(server server/main.c utils/types.h utils/connection_utils.c utils/connection_utils.h utils/err.c utils/err.h utils/command_utils.c utils/command_utils.h utils/args_utils.c utils/args_utils.h utils/user_input_output.c utils/user_input_output.h utils/file_utils.c utils/file_utils.h)
add_executable(client client/main.c utils/types.h utils/connection_utils.c utils/connection_utils.h utils/err.c utils/err.h utils/command_utils.c utils/command_utils.h utils/args_utils.c utils/args_utils.h utils/user_input_output.c utils/user_input_output.h utils/file_utils.c utils/file_utils.h)