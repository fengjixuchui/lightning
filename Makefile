test:
	g++ -o test1 test.cc -O4 -pthread -w
	g++ -o test2 test2.cc -O4 -pthread -w
	./test1
	./test2

why:
	g++ -o programA programA.cc -O4 -pthread -w
	./programA
