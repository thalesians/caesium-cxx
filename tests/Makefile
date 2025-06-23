all: test_sodium test_time memory/release-sink-machinery memory/switch-memory memory/promise-memory performance

SRC=..
BOOST_INC := $(shell brew --prefix boost)/include
CPPUNIT_INC := $(shell brew --prefix cppunit)/include
CPPUNIT_LIB := $(shell brew --prefix cppunit)/lib
CPPFLAGS=-I$(SRC) -I$(BOOST_INC) -I$(CPPUNIT_INC) -g -Wshadow -Werror -Wno-deprecated-declarations --std=c++11
LDFLAGS=-L$(CPPUNIT_LIB)

#CPPFLAGS+=-DSODIUM_SINGLE_THREADED
#CPPFLAGS+=-DSODIUM_USE_PTHREAD_SPECIFIC

# Strangely on g++-4.9.2 for ARM, using thread_local storage class doesn't
# work and SODIUM_USE_PTHREAD_SPECIFIC is needed.
# Test cases work but a big commercial app will a million threads breaks.

OBJECT_FILES= \
    $(SRC)/sodium/lock_pool.o \
    $(SRC)/sodium/light_ptr.o \
    $(SRC)/sodium/transaction.o \
    $(SRC)/sodium/time.o \
    $(SRC)/sodium/sodium.o

SODIUM_HEADERS=\
    $(SRC)/sodium/sodium.h \
    $(SRC)/sodium/transaction.h \
    $(SRC)/sodium/light_ptr.h \
    $(SRC)/sodium/count_set.h \
    $(SRC)/sodium/lock_pool.h \
    $(SRC)/sodium/sodium_base.h

$(SRC)/sodium/light_ptr.o:       $(SRC)/sodium/light_ptr.h $(SRC)/sodium/lock_pool.h
$(SRC)/sodium/transaction.o:     $(SRC)/sodium/transaction.h $(SRC)/sodium/lock_pool.h $(SRC)/sodium/light_ptr.h $(SRC)/sodium/count_set.h
$(SRC)/sodium/sodium.o:          $(SODIUM_HEADERS)
$(SRC)/sodium/time.o:            $(SODIUM_HEADERS)
test_sodium.o:                   $(SODIUM_HEADERS) test_sodium.h
test_time.o:                     $(SODIUM_HEADERS)
performance.o:                   $(SODIUM_HEADERS)
memory/release-sink-machinery.o: $(SODIUM_HEADERS)
memory/switch-memory.o:          $(SODIUM_HEADERS)

test_sodium: $(OBJECT_FILES) test_sodium.o
	$(CXX) $(CPPFLAGS) -o $@ $(OBJECT_FILES) test_sodium.o $(LDFLAGS) -lpthread -lcppunit

test_time: $(OBJECT_FILES) test_time.o
	$(CXX) -o $@ $(OBJECT_FILES) test_time.o -lpthread

main: $(OBJECT_FILES) main.o
	$(CXX) -o $@ $(OBJECT_FILES) main.o -lpthread

memory/release-sink-machinery: $(OBJECT_FILES) memory/release-sink-machinery.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/release-sink-machinery.o -lpthread

memory/switch-memory: $(OBJECT_FILES) memory/switch-memory.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/switch-memory.o -lpthread

memory/promise-memory: $(OBJECT_FILES) memory/promise-memory.o
	$(CXX) -o $@ $(OBJECT_FILES) memory/promise-memory.o -lpthread

performance: $(OBJECT_FILES) performance.o
	$(CXX) -o $@ $(OBJECT_FILES) performance.o -lpthread

clean:
	rm -f $(OBJECT_FILES) \
            test_sodium test_sodium.o test_time.o \
            memory/release-sink-machinery memory/release-sink-machinery.o \
            memory/switch-memory memory/switch-memory.o \
            memory/promise-memory memory/promise-memory.o \
            performance performance.o
