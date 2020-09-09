include prorab.mk

this_name := tests

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format
this_cxxflags += -Wno-format-security
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing
this_cxxflags += -std=c++11

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/build/libsvgren$(dot_so) -lpapki `pkg-config --libs cairo` -lpng -lstdc++ -lpthread

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ # this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
endif

this_no_install := true

$(eval $(prorab-build-app))

this_dirs := $(subst /, ,$(d))
this_test := $(word $(words $(this_dirs)),$(this_dirs))

this_svgs := $(shell ls $(d)../samples/testdata/*.svg)

define this_rules
test:: $(prorab_this_name)
$(.RECIPEPREFIX)@myci-running-test.sh $(this_test)
$(.RECIPEPREFIX)$(a)cp $(d)../../src/build/*.dll . || true
$(.RECIPEPREFIX)$(a)LD_LIBRARY_PATH=$(d)../../src/build DYLD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH $(d)./tests $(this_svgs); \
        if [ $$$$? -ne 0 ]; then myci-error.sh "test failed"; exit 1; fi
$(.RECIPEPREFIX)@myci-passed.sh
endef
# TODO: enable test
# NOTE: this test is turned off because there is some problem with it when running under qemu (raspberry pi) on CI machine
#$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/build/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
