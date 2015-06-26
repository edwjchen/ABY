ABY=aby
TEST=test
LOWMC=lowmc
MILLIONAIRE=millionaire
BIN=bin
SRC=src
CORE=${SRC}/abycore

# compiler settings
CC=g++
COMPILER_OPTIONS=-O2
DEBUG_OPTIONS=-g3 -Wall -Wextra
BATCH=

ARCHITECTURE = $(shell uname -m)
ifeq (${ARCHITECTURE},x86_64)
MIRACL_MAKE:=linux64
GNU_LIB_PATH:=x86_64
else
MIRACL_MAKE:=linux
GNU_LIB_PATH:=i386
endif

INCLUDE=-I..  -I/usr/include/glib-2.0/ -I/usr/lib/${GNU_LIB_PATH}-linux-gnu/glib-2.0/include


LIBRARIES=-lgmp -lgmpxx -lpthread ${CORE}/util/miracl_lib/miracl.a -L /usr/lib  -lssl -lcrypto -lglib-2.0
CFLAGS=

# directory for the Miracl submodule and library
MIRACL_LIB_DIR=${CORE}/util/miracl_lib
SOURCES_MIRACL=${CORE}/util/Miracl/*
OBJECTS_MIRACL=${MIRACL_LIB_DIR}/*.o


# all source files and corresponding object files in abycore
SOURCES_CORE := $(shell find ${CORE} -type f -name '*.cpp' -not -path '*/util/Miracl/*')
OBJECTS_CORE := $(SOURCES_CORE:.cpp=.o)

# objects in example (sub-)folders
OBJECTS_EXAMPLE = $(shell find ${SRC}/examples -type f -name '*.o')

#objects in test (sub-) folders
OBJECTS_TEST = $(shell find ${SRC}/${TEST} -type f -name '*.o')

# all sub-directories of ${SRC}/examples with their full path
EXAMPLE_SUBDIRS := $(realpath $(wildcard ${SRC}/examples/*/.))

all: miracl core examples ${TEST}
	@echo "make all done."

# this will create a copy of the files in src/util/Miracl and its sub-directories and put them into src/util/miracl_lib without sub-directories, then compile it
miracl:	${MIRACL_LIB_DIR}/miracl.a

# copy Miracl files to a new directory (${CORE}/util/miracl_lib/), call the build script and delete everything except the archive, header and object files.
${MIRACL_LIB_DIR}/miracl.a: ${SOURCES_MIRACL}
	@find ${CORE}/util/Miracl/ -type f -exec cp '{}' ${CORE}/util/miracl_lib \;
	@cd ${CORE}/util/miracl_lib/; bash ${MIRACL_MAKE}; find . -type f -not -name '*.a' -not -name '*.h' -not -name '*.o' -not -name '.git*'| xargs rm

core: ${OBJECTS_CORE}

%.o:%.cpp %.h
	${CC} $< ${COMPILER_OPTIONS} -c ${INCLUDE} ${CFLAGS} ${BATCH} -o $@

# check that core is built, then call test makefile
${TEST}: ${OBJECTS_CORE}
	@(cd ${SRC}/${TEST}; if [ -e Makefile ]; then $(MAKE); fi)

# this will run the previously compiled test-aby executables #TODO: take care of the output and errors
runtest: ${TEST}
	${BIN}/test-aby.exe -r 0 &
	${BIN}/test-aby.exe -r 1

examples: ${OBJECTS_CORE} ${EXAMPLE_SUBDIRS}

# if there is a Makefile for an example, then make it
${EXAMPLE_SUBDIRS}:
	@(cd $@; if [ -e Makefile ]; then $(MAKE) -C $@; fi)

.PHONY: clean cleanall examples ${EXAMPLE_SUBDIRS} all ${TEST} miracl runtest core

# only clean example objects, test object and binaries
clean:
	rm -f ${OBJECTS_EXAMPLE} ${OBJECTS_TEST} ${BIN}/*.exe
# clean example objects, test object, aby core objects and binaries
cleanmore: clean
	rm -f ${OBJECTS_CORE}

# this will clean everything: example objects, test object and binaries and the Miracl library
cleanall: cleanmore
	rm -f ${OBJECTS_MIRACL} ${MIRACL_LIB_DIR}/*.a
