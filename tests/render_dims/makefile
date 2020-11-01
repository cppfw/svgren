include prorab.mk

this_name := tests

this_srcs += main.cpp

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom $(d)../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lstdc++
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
$(.RECIPEPREFIX)$(a)cp $(d)../../src/out/$(c)/*.dll $(d) || true
$(.RECIPEPREFIX)$(a)(cd $(d) && LD_LIBRARY_PATH=../../src/out/$(c) DYLD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH out/$(c)/tests); \
        if [ $$$$? -ne 0 ]; then myci-error.sh "test failed"; exit 1; fi
$(.RECIPEPREFIX)@myci-passed.sh
endef
$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
