CFLAGS=-I${HEADERS}
HEADERS=../include

object=epollServer

all: ${object}

${object}: server.c
	cc ${CFLAGS} server.c -o ${object}


.PHONY : clean
clean:
	rm epollServer
