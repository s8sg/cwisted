all: myProgram

myProgram: #libmylib.a is the dependency for the executable
	gcc -c -Iinclude/twisted.h src/twisted.c

clean:
	rm -f twisted *.o *.a *.gch #This way is cleaner than your clean
