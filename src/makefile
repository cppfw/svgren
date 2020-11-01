include prorab.mk

this_name := svgren

this_soname := $(shell cat $(d)soname.txt)

this_srcs := $(call prorab-src-dir,.)

# this_cxxflags += -D SVGREN_BACKGROUND=0xffffffff
# this_cxxflags += -D SVGREN_BACKGROUND=0xff00ffff

$(eval $(call prorab-config, ../config))

ifeq ($(this_svgren_backend),agg)
    this_cxxflags += -D SVGREN_BACKEND=SVGREN_BACKEND_AGG
    this_ldlibs += `pkg-config --libs agg`
else ifeq ($(this_svgren_backend),cairo)
    this_cxxflags += -D SVGREN_BACKEND=SVGREN_BACKEND_CAIRO
    ifeq ($(os),windows)
        this_cxxflags += `pkg-config --cflags cairo`
    endif
    this_ldlibs += `pkg-config --libs cairo`
endif

ifeq ($(debug),true)
    this_cxxflags += -DDEBUG
else
    this_cxxflags += -O3
endif

ifeq ($(gprof),true)
    this_cxxflags += -pg
    this_ldflags += -pg
endif

this_ldlibs += -lsvgdom -lstdc++ -lm

ifeq ($(os),linux)
    this_cxxflags += -fPIC # since we are building shared library, we need Position-Independend Code
else ifeq ($(os),windows)
else ifeq ($(os),macosx)
    this_cxxflags += -stdlib=libc++ # this is needed to be able to use c++11 std lib
endif

$(eval $(prorab-build-lib))
