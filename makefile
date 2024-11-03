CC = g++ -g
CCFLAGS = -std=c++23 -Wall -Wextra

LEX_YY = scanner.c
Y_TAB = parser.cpp


all: build

parser.c: parser.y
	@bison -Wcounterexamples -d parser.y -o $(Y_TAB)

scanner.c: scanner.l parser.c
	@flex -o $(LEX_YY) scanner.l 

build: parser.c scanner.c command.cpp
	@$(CC) $(CCFLAGS) $(LEX_YY) $(Y_TAB) command.cpp  -o kirosh

run: build
	@./kirosh

clean:
	@rm kirosh $(LEX_YY) $(Y_TAB)
