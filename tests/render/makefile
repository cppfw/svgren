include prorab.mk

this_name := tests

ifeq ($(gprof), true)
    this_cxxflags += -pg
    this_ldflags += -pg
endif

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format
this_cxxflags += -Wno-format-security
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing
this_cxxflags += -std=c++14

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/build/libsvgren$(dot_so) -lpapki -lpng -lstdc++
this_ldflags += -L$(d)../../src

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ # this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
else
    this_ldlibs += -lX11
endif

this_no_install := true

$(eval $(prorab-build-app))

ifeq ($(os),windows)
    this_run_cmd := $(a)cp $(d)../../src/build/libsvgren$(dot_so) $(d) && $$^)
else
    this_run_cmd := $(a)(cd $(d); LD_LIBRARY_PATH=../../src/build $$^)
endif

define this_rules
run:: $(prorab_this_name)
$(.RECIPEPREFIX)@echo running $$^...
$(.RECIPEPREFIX)$(a)cp $(d)../../src/build/libsvgren$(dot_so) . || true
$(.RECIPEPREFIX)$(a)LD_LIBRARY_PATH=$(d)../../src/build $(d)./tests $(svg)
endef
$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/build/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
