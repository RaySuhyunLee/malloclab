default:
	gcc -O2 -Wall -shared -fPIC -o mc.so mc.c -ldl
	gcc -Wall -o test test.c

run:
	LD_PRELOAD="./mc.so" ./test

clean:
	rm *.so test
