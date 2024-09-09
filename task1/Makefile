SOURCE = sieve_of_eratothenes

all:
	../../qbe/obj/qbe -o $(SOURCE).s $(SOURCE).qbe
	gcc $(SOURCE).s test.c -o test
	./test