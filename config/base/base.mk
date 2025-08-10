this_cxxflags += -Wall # enable all warnings
this_cxxflags += -Wnon-virtual-dtor # warn if base class has non-virtual destructor
this_cxxflags += -Werror # warnings as errors
this_cxxflags += -Wfatal-errors # stop on first error encountered
this_cxxflags += -fstrict-aliasing # in order to comply with the c++ standard more strictly
this_cxxflags += -std=c++17
this_cxxflags += -g
this_cxxflags += -fPIC

this_ldlibs += -lstdc++

this_no_format_test := true

ifeq ($(gprof), true)
    this_cxxflags += -pg
    this_ldflags += -pg
endif
