all:
	touch tmp/dummy.txt
	rm tmp/*	
	gcc main.c -o main
	gcc server.c include/log.c -o server
	gcc userinterface.c include/log.c -lncurses -o userinterface
	gcc drone.c include/log.c -o drone
	gcc watchdog.c include/log.c -o watchdog
	touch tmp/dummy.txt
	rm tmp/*	
	gcc main.c include/log.c -o main
	gcc server.c include/log.c -o server
	gcc userinterface.c include/log.c -lncurses -o userinterface
	gcc drone.c include/log.c -o drone
	gcc watchdog.c include/log.c -o watchdog
	sleep 2
	./main

	

