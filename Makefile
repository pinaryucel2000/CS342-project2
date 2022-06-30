all: schedule

schedule: schedule.c
	gcc -o schedule schedule.c -pthread -lm

clean: 
	rm fr schedule schedule.o *~
