# Compiler and linker configurations
CC = g++
CFLAGS = -Wall -g
LDFLAGS = -pthread  # Linking with pthread library

# Define the names of the output executable
EXEC = simulator

# Object files
OBJS = main.o helper.o WriteOutput.o

# Rule to make everything
all: $(EXEC)

# Rule to make the executable
$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Rule to compile main.cpp
main.o: main.cpp helper.h WriteOutput.h monitor.h
	$(CC) $(CFLAGS) -c main.cpp

# Rule to compile helper.c
helper.o: helper.c helper.h
	$(CC) $(CFLAGS) -c helper.c

# Rule to compile WriteOutput.c
WriteOutput.o: WriteOutput.c WriteOutput.h
	$(CC) $(CFLAGS) -c WriteOutput.c

# Clean rule
clean:
	rm -f $(EXEC) $(OBJS)

# Rule to clean and then make everything
rebuild: clean all
