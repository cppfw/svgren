include prorab.mk

this_name := svgren

this_soname := $(shell cat $(d)soname.txt)

this_out_dir := build

this_srcs := $(call prorab-src-dir,.)

this_cxxflags += -Wall # enable all warnings
this_cxxflags += -Wno-comment # no warnings on nested comments
this_cxxflags += -funsigned-char # the 'char' type is unsigned
this_cxxflags += -Wnon-virtual-dtor # warn if base class has non-virtual destructor
this_cxxflags += -fno-operator-names # do not treat 'and', 'bitand','bitor','compl','not','or','xor' as keywords
this_cxxflags += -Werror # treat warnings as errors
this_cxxflags += -Wfatal-errors # stop on first error encountered
this_cxxflags += -fstrict-aliasing # in order to comply with the c++ standard more strictly
this_cxxflags += -std=c++14
this_cxxflags += -g

ifeq ($(debug),true)
    this_cxxflags += -DDEBUG
else
    this_cxxflags += -O3
endif

ifeq ($(gprof), true)
    this_cxxflags += -pg
    this_ldflags += -pg
endif

this_ldlibs += `pkg-config --libs cairo` -lsvgdom -lstdc++ -lm

ifeq ($(os),linux)
    this_cxxflags += -fPIC # Since we are building shared library, we need Position-Independend Code
else ifeq ($(os),windows)
    this_cxxflags += `pkg-config --cflags cairo`
else ifeq ($(os),macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
endif

$(eval $(prorab-build-lib))
