all:
	rm tmp/*	
	gcc main.c -o main
	gcc server.c -o server
	gcc userinterface.c -lncurses -o userinterface
	gcc drone.c -o drone
	gcc watchdog.c -o watchdog

	

