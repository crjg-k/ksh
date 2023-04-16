CC=x86_64-linux-gnu-gcc
TARGET=ksh
TEST=test
INCLUDE_DIR=./include
HEAD_FILES=$(INCLUDE_DIR)/*
CFLAGS= -g -fsanitize=address -fno-builtin

all: $(TARGET) clean
testf: $(TEST) clean

$(TARGET): main.o util.o
	$(CC) $^ -I $(INCLUDE_DIR) $(CFLAGS) -o $(TARGET)

$(TEST): test.o util.o
	$(CC) $^ -I $(INCLUDE_DIR) $(CFLAGS) -o $(TEST)

util.o: util/util.c $(HEAD_FILES)
	$(CC) $< -I $(INCLUDE_DIR) $(CFLAGS) -c
main.o: main.c $(HEAD_FILES)
	$(CC) $< -I $(INCLUDE_DIR) $(CFLAGS) -c
test.o: test.c $(HEAD_FILES)
	$(CC) $< -I $(INCLUDE_DIR) $(CFLAGS) -c

clean:
	rm *.o
