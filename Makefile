all:
	gcc -w -pthread simulator.c helper.c writeOutput.c -o simulator
	./simulator < input.txt

