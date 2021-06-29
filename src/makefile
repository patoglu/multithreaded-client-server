CXX = gcc
CXXFLAGS = 	
LDFLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-stringop-truncation
LBLIBS = -lrt -pthread -g

SRC = server.c robust_io.c helper.c alloc.c queue.c
SRC_CLIENT = client.c robust_io.c helper.c alloc.c
OBJ = $(SRC:.cc=.o)
OBJ_CLIENT = $(SRC_CLIENT:.cc=.o)
EXEC = server
EXEC_CLIENT = client

all: $(EXEC) $(EXEC_CLIENT)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LBLIBS)
$(EXEC_CLIENT): $(OBJ_CLIENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_CLIENT) $(LBLIBS)

clean:
	rm server 
	rm client
