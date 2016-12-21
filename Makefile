GCC     = g++
CFLAGS = -lwiringPi

INCDIR = include
SRCDIR = source

OBJS   = $(SRCDIR)/FileHandling.o $(SRCDIR)/ini.o $(SRCDIR)/INIReader.o $(SRCDIR)/Controller.o

B0Shim: B0Shim.cpp $(OBJS)
	$(GCC) -o $@ $^ $(CFLAGS)

FileHandling.o: $(SRCDIR)/FileHandling.o $(SRCDIR)/FileHandling.cpp
	$(GCC) -c $(SRCDIR)/FileHandling.cpp

ini.o: $(SRCDIR)/ini.o $(SRCDIR)/ini.c
	$(GCC) -c $(SRCDIR)/ini.c
	
INIReader.o: $(SRCDIR)/INIReader.o $(SRCDIR)/INIReader.cpp
	$(GCC) -c $(SRCDIR)/INIReader.cpp
	
Controller.o: $(SRCDIR)/Controller.o $(SRCDIR)/Controller.cpp
	$(GCC) -c $(SRCDIR)/Controller.cpp
	
clean:
	rm -f $(SRCDIR)/*.o $(SRCDIR)/*~ $(INCDIR)/*~ *~ B0Shim
