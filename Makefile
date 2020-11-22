# Tariq Alturkestani 
# Tariq.Alturkestani@kaust.edu.sa
# November 19, 2020 
TAR_NAME="MLBS-$(shell date +'%Y.%m.%d-%H-%M-%S')"
OBJS=  Storage.o
INC_PATH= 
LIBS = 
CC = g++      
CFLAGS = -g -O3 --std=c++11  -fopenmp
all: $(OBJS) fakeRTM
fakeRTM: $(OBJS) driver.cpp
	$(CC) $(CFLAGS)  $(OBJS) $(LIBS) driver.cpp -o $@ 
$(OBJS): %.o :%.cpp
	$(CC) $(CFLAGS) -c $< $(LIBS) $(INC_PATH)
tar:
	tar -czvf $(TAR_NAME).tar.gz *.cpp *.h   Makefile* 
clean:
	rm -f  fakeRTM  a.out *.o *.out *.err  *.optrpt *~
