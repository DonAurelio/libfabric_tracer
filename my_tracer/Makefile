SRC231   = $(abspath ../src/libfabric-2.3.1)
MODINC   = /opt/cray/libfabric/2.3.1/include
MODLIB   = /opt/cray/libfabric/2.3.1/lib64

CC       = gcc
CFLAGS   = -shared -fPIC -O0 -g \
           -D_GNU_SOURCE \
           -I$(SRC231)/include \
           -I$(SRC231)/prov/hook/include \
           -I$(SRC231)/prov/hook/perf/include \
           -I$(SRC231) \
           -I$(MODINC)
LDFLAGS  = -L$(MODLIB) -Wl,-rpath,$(MODLIB)
LIBS     = -lfabric

# Hook infrastructure source files compiled into the DSO
HOOK_SRCS = $(SRC231)/prov/hook/src/hook.c \
            $(SRC231)/prov/hook/src/hook_xfer.c \
            $(SRC231)/prov/hook/src/hook_ep.c \
            $(SRC231)/prov/hook/src/hook_cm.c \
            $(SRC231)/prov/hook/src/hook_domain.c \
            $(SRC231)/prov/hook/src/hook_av.c \
            $(SRC231)/prov/hook/src/hook_cq.c \
            $(SRC231)/prov/hook/src/hook_eq.c \
            $(SRC231)/prov/hook/src/hook_wait.c \
            $(SRC231)/prov/hook/src/hook_cntr.c

DSO = libmytracer_fi.so

all: $(DSO)

$(DSO): my_tracer.c $(HOOK_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(DSO)

# Run the pingpong example with our tracer active
run: $(DSO)
	FI_PROVIDER_PATH=$(CURDIR) \
	FI_PROVIDER=tcp \
	FI_HOOK=mytracer \
	../examples/pingpong 2>&1

.PHONY: all clean run
