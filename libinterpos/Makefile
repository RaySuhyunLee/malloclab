default: $(objects)
	gcc -O2 -Wall -shared -fPIC -o mc.so mc.c -ldl
	gcc -Wall -o test.out test.c

run: 
	LD_PRELOAD="./mc.so" ./test.out

clean:
	rm *.so *.out
