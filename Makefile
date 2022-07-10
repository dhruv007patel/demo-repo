all:lets-talk

lets-talk:lets-talk.c list.c list.h
	gcc -g -Wall -o lets-talk lets-talk.c -lpthread list.c list.h

valgrind:
	valgrind --leak-check=full \
			 --show-leak-kinds=all \
			 --track-origins=yes \
			 --verbose \
			 ./lets-talk 3010 localhost 3000
	
clean:
	$(RM) lets-talk *.o