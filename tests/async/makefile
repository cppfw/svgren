include prorab.mk

this_name := tests

this_srcs += main.cpp

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom $(d)../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lpng -lstdc++ -lpthread

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
$(.RECIPEPREFIX)$(a)cp $(d)../../src/out/$(c)/*.dll . || true
$(.RECIPEPREFIX)$(a)LD_LIBRARY_PATH=$(d)../../src/out/$(c) DYLD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH $(d)out/$(c)/tests $(this_svgs); \
        if [ $$$$? -ne 0 ]; then myci-error.sh "test failed"; exit 1; fi
$(.RECIPEPREFIX)@myci-passed.sh
endef
# TODO: enable test
# NOTE: this test is turned off because there is some problem with it when running under qemu (raspberry pi) on CI machine
# $(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
