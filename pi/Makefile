TARGET = client
SOURCE = piclient.c
CFLAGS = -lpthread -std=c99 -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)
	
clean:
	rm -fr $(TARGET)
	
