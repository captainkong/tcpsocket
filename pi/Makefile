TARGET = client
SOURCE = piclient.c
CFLAGS = -lpthread -lssl  -lcrypto -D_GNU_SOURCE	-L../lib/cJSON -lcjson -lm -std=c99 -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)
	
clean:
	rm -fr $(TARGET)
	
