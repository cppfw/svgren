include prorab.mk

this_name := checksvg

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format #no warnings about format
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -std=c++11

this_srcs += checksvg.cpp

this_ldlibs += -lsvgdom -lsvgren -lpapki -lpng -lstdc++
this_ldflags += -L$(d)../../src

ifeq ($(prorab_os), macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(prorab_os),windows)
    this_cxxflags += -I/usr/include -I/usr/local/include
    this_ldflags += -L/usr/lib -L/usr/local/lib
endif

$(eval $(prorab-build-app))

#add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/libsvgren$(prorab_lib_extension))

$(eval $(call prorab-include,$(d)../../src/makefile))
