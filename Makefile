CC = g++
CFLAGS = -Wall -g
TARGET = server
OBJS = httpcon.o locker.o main.o sem.o timer_list.o timer.o log.o access_control.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CXXFLAGS) -o $(TARGET) $(OBJS) -lpthread 

httpcon.o: httpcon.cpp httpcon.h timer.h log.h
	$(CC) $(CXXFLAGS) -c httpcon.cpp

locker.o: locker.cpp locker.h
	$(CC) $(CXXFLAGS) -c locker.cpp

log.o: log.cpp log.h locker.h
	$(CC) $(CXXFLAGS) -c log.cpp

main.o: main.cpp macro.h httpcon.h pool.h timer_list.h timer.h log.h
	$(CC) $(CXXFLAGS) -c main.cpp

access_control.o: access_control.cpp access_control.h
	$(CC) $(CXXFLAGS) -c access_control.cpp

sem.o: sem.cpp sem.h
	$(CC) $(CXXFLAGS) -c sem.cpp

timer_list.o: timer_list.cpp timer_list.h timer.h
	$(CC) $(CXXFLAGS) -c timer_list.cpp

timer.o: timer.cpp timer.h
	$(CC) $(CXXFLAGS) -c timer.cpp

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean@
