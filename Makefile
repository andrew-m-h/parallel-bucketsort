CFLAGS=-Wall -Wextra -Werror -O3 -pipe -x c -std=c99

bucketsort: bucketsort.c bucketsort.h
	mpicc $(CFLAGS) -o bucketsort bucketsort.c -I. -DOUTPUT #-DDEBUG

gen_rand: gen_rand.c
	gcc $(CFLAGS) -o gen_rand gen_rand.c

clean:
	rm -f bucketsort gen_rand
