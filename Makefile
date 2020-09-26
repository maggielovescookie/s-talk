all: s-talk

s-talk:
	gcc -std=c11 -pthread -o s-talk instructorList.o communication.c s-talk.c -Wall -Werror

clean:
	rm -f s-talk
