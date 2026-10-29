INSTALL = /home/avivasmeza/dev/libfabric_tracer/install
CC      = gcc
CFLAGS  = -g -O0 -I$(INSTALL)/include
LDFLAGS = -L$(INSTALL)/lib -Wl,-rpath,$(INSTALL)/lib
LIBS    = -lfabric

all: pingpong

pingpong: pingpong.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f pingpong

run: pingpong
	LD_LIBRARY_PATH=$(INSTALL)/lib:$$LD_LIBRARY_PATH \
	FI_PROVIDER=tcp FI_HOOK=trace FI_LOG_LEVEL=trace \
	./pingpong 2>&1 | tee ../traces/trace_output.txt

run-debug: pingpong
	LD_LIBRARY_PATH=$(INSTALL)/lib:$$LD_LIBRARY_PATH \
	FI_PROVIDER=tcp FI_HOOK=trace FI_LOG_LEVEL=debug \
	./pingpong 2>&1 | tee ../traces/debug_output.txt

.PHONY: all clean run run-debug
