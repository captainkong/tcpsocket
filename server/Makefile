TARGET = tcp_echo_srv
SOURCE = tcp_echo_srv.c
CFLAGS = -Wall
CC = gcc

$(TARGET):$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS)

clean:
	rm -fr $(TARGET)
