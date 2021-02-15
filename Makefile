websnarf: websnarf.o
	gcc -o websnarf -lnsl websnarf.o
websnarf.o: websnarf.c
	gcc -c websnarf.c


