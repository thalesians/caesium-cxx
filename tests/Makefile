ifeq ($(NO_CXX11),)
all: test_sodium memory/release-sink-machinery memory/switch-memory
else
all: test_sodium
endif

SRC=../v1
CPPFLAGS=-I$(SRC) -g -Wshadow -Werror
ifeq ($(NO_CXX11),)
CPPFLAGS+=--std=c++0x
else
CPPFLAGS+=--std=c++03 -DSODIUM_NO_CXX11
endif

OBJECT_FILES= \
    $(SRC)/sodium/lock_pool.o \
    $(SRC)/sodium/light_ptr.o \
    $(SRC)/sodium/transaction.o \
    $(SRC)/sodium/sodium.o

SODIUM_HEADERS=$(SRC)/sodium/sodium.h $(SRC)/sodium/transaction.h $(SRC)/sodium/light_ptr.h $(SRC)/sodium/count_set.h $(SRC)/sodium/lock_pool.h

$(SRC)/sodium/light_ptr.o:       $(SRC)/sodium/light_ptr.h $(SRC)/sodium/lock_pool.h
$(SRC)/sodium/transaction.o:     $(SRC)/sodium/transaction.h $(SRC)/sodium/lock_pool.h $(SRC)/sodium/light_ptr.h $(SRC)/sodium/count_set.h
$(SRC)/sodium/sodium.o:          $(SODIUM_HEADERS)
test_sodium.o:                   $(SODIUM_HEADERS) test_sodium.h
memory/release-sink-machinery.o: $(SODIUM_HEADERS)
memory/switch-memory.o:          $(SODIUM_HEADERS)

test_sodium: $(OBJECT_FILES) test_sodium.o
	$(CXX) -o $@ $(OBJECT_FILES) test_sodium.o -lpthread -lcppunit

memory/release-sink-machinery: $(OBJECT_FILES) memory/release-sink-machinery.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/release-sink-machinery.o -lpthread

memory/switch-memory: $(OBJECT_FILES) memory/switch-memory.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/switch-memory.o -lpthread

clean:
	rm -f $(OBJECT_FILES) \
            test_sodium test_sodium.o \
            memory/release-sink-machinery memory/release-sink-machinery.o
