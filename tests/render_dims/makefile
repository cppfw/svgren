include prorab.mk

this_name := tests

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format
this_cxxflags += -Wno-format-security
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing
this_cxxflags += -std=c++14

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/build/libsvgren$(dot_so) -lpapki -lstdc++
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

this_dirs := $(subst /, ,$(d))
this_test := $(word $(words $(this_dirs)),$(this_dirs))

define this_rules
test:: $(prorab_this_name)
$(.RECIPEPREFIX)@myci-running-test.sh $(this_test)
$(.RECIPEPREFIX)$(a)cp $(d)../../src/build/*.dll $(d) || true
$(.RECIPEPREFIX)$(a)(cd $(d) && LD_LIBRARY_PATH=../../src/build DYLD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH ./tests); \
        if [ $$$$? -ne 0 ]; then myci-error.sh "test failed"; exit 1; fi
$(.RECIPEPREFIX)@myci-passed.sh
endef
$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/build/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
