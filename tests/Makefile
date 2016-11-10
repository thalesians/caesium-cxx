all: test_sodium test_time memory/release-sink-machinery memory/switch-memory memory/promise-memory

SRC=..
CPPFLAGS=-I$(SRC) -g -Wshadow -Werror --std=c++11
#CPPFLAGS+=-DSODIUM_SINGLE_THREADED

OBJECT_FILES= \
    $(SRC)/sodium/lock_pool.o \
    $(SRC)/sodium/light_ptr.o \
    $(SRC)/sodium/transaction.o \
    $(SRC)/sodium/time.o \
    $(SRC)/sodium/sodium.o

SODIUM_HEADERS=$(SRC)/sodium/sodium.h $(SRC)/sodium/transaction.h $(SRC)/sodium/light_ptr.h $(SRC)/sodium/count_set.h $(SRC)/sodium/lock_pool.h

$(SRC)/sodium/light_ptr.o:       $(SRC)/sodium/light_ptr.h $(SRC)/sodium/lock_pool.h
$(SRC)/sodium/transaction.o:     $(SRC)/sodium/transaction.h $(SRC)/sodium/lock_pool.h $(SRC)/sodium/light_ptr.h $(SRC)/sodium/count_set.h
$(SRC)/sodium/sodium.o:          $(SODIUM_HEADERS)
$(SRC)/sodium/time.o:            $(SODIUM_HEADERS)
test_sodium.o:                   $(SODIUM_HEADERS) test_sodium.h
test_time.o:                     $(SODIUM_HEADERS)
memory/release-sink-machinery.o: $(SODIUM_HEADERS)
memory/switch-memory.o:          $(SODIUM_HEADERS)

test_sodium: $(OBJECT_FILES) test_sodium.o
	$(CXX) -o $@ $(OBJECT_FILES) test_sodium.o -lpthread -lcppunit

test_time: $(OBJECT_FILES) test_time.o
	$(CXX) -o $@ $(OBJECT_FILES) test_time.o -lpthread

memory/release-sink-machinery: $(OBJECT_FILES) memory/release-sink-machinery.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/release-sink-machinery.o -lpthread

memory/switch-memory: $(OBJECT_FILES) memory/switch-memory.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/switch-memory.o -lpthread

memory/promise-memory: $(OBJECT_FILES) memory/promise-memory.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/promise-memory.o -lpthread

clean:
	rm -f $(OBJECT_FILES) \
            test_sodium test_sodium.o test_time.o \
            memory/release-sink-machinery memory/release-sink-machinery.o \
            memory/switch-memory memory/switch-memory.o \
            memory/promise-memory memory/promise-memory.o
