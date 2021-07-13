
CC = gcc
CXX = g++
CXXFLAGS = -D__STDC_LIMIT_MACROS
CFLAGS = 
LDFLAGS = -rdynamic
LDLIBS = -lpthread

.PHONY: all
all: scom

# Actual targets
scom: scom.o
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)

# implicit rules
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
%: %.o
	$(CXX) $(LDFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	-rm -f *.o
	-rm -f scom

.PHONY: cleanall
cleanall: clean

