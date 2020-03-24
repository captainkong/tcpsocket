TARGET = serverd
SOURCE = server.c
CFLAGS = -lpthread -lssl  -lcrypto	-L../lib/cJSON -lcjson -lm -I/usr/include/mysql/  -L/usr/lib64/mysql/ -lmysqlclient -std=c99 -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)
	
clean:
	rm -fr $(TARGET)
	
