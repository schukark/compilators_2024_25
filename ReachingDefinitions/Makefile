compile:
	clang++ -std=c++17 -I/usr/lib/qbe/include -L/usr/lib/qbe/lib main.cxx -lqbe -o main

run: compile
	./main

valgrind: compile
	valgrind --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file=valgrind-out.txt \
        ./main