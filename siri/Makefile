TARGET = siri
SOURCE = siri.c
CFLAGS = -lpthread -std=c99 -L../lib/cJSON -lcjson -lm -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)
#把可执行文件复制到系统目录
	echo yes | cp $(TARGET) /usr/local/bin/
	
clean:
	rm -fr $(TARGET)