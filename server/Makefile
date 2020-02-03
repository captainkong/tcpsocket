TARGET = serverd
SOURCE = server.c
CFLAGS = -lpthread	-L../lib/cJSON -lcjson -lm -std=c99 -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)
	
clean:
	rm -fr $(TARGET)
	
