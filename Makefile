server: main.cpp ./Lock/Lockers.h
	g++ -o server main.cpp ./Lock/Lockers.h

clean:
	rm -r server