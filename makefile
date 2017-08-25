CC := gcc
FLAGS := -Wall

CLIENT_LIB := -L./Commons -lcommons -lpthread -lcrypto -lssl
SERVER_LIB := -L./Commons -lcommons -lpthread -lcrypto -lssl
CMMONS_LIB := -lpthread -lcrypto -lssl

CLIENT_SRC := $(wildcard Client/*.c)
SERVER_SRC := $(wildcard Server/*.c)
CMMONS_SRC := $(wildcard Commons/*.c)

CMMONS_OBJ := $(patsubst %.c, %.o, $(CMMONS_SRC));

HEADERS_DIR := -I./Commons

#-------------------------------------------------------------------------

all: client server

client: commons
	${CC} -o Client/client ${FLAGS} ${CLIENT_SRC} ${CLIENT_LIB} ${HEADERS_DIR}

server: commons
	${CC} -o Server/server ${FLAGS} ${SERVER_SRC} ${SERVER_LIB} ${HEADERS_DIR}

commons: $(CMMONS_OBJ)
	ar ru Commons/libcommons.a $(CMMONS_OBJ)
	ranlib Commons/libcommons.a
	rm $(CMMONS_OBJ)

Commons/%.o: Commons/%.c
	${CC} -c $< -o $@ $(CMMONS_LIBS)

clean:
	rm -f Commons/libcommons.a
	rm -f Client/client
	rm -f Client/metadata
	rm -f Server/server
	rm -f Server/metadata

.PHONY: clean
