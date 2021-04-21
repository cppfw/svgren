include prorab.mk
include prorab-test.mk
include myci.mk

$(eval $(call myci-try-include, $(CONANBUILDINFO_DIR)conanbuildinfo.mak))

this_name := tests

$(eval $(call prorab-config, ../../config))

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lpng -lutki $(addprefix -l,$(CONAN_LIBS))
this_ldflags += -L$(d)../../src

this_cxxflags += $(addprefix -I,$(CONAN_INCLUDE_DIRS))
this_ldflags += $(addprefix -L,$(CONAN_LIB_DIRS))

ifeq ($(os), macosx)
else ifeq ($(os),windows)
else
    this_ldlibs += -lX11
endif

this_no_install := true

$(eval $(prorab-build-app))

ifeq ($(os),windows)
    this_run_cmd := $(a)cp $(d)../../src/out/$(c)/libsvgren$(dot_so) $(d) && $$^)
else
    this_run_cmd := $(a)(cd $(d); LD_LIBRARY_PATH=../../src/out/$(c) $$^)
endif

define this_rules
    run:: $(prorab_this_name)
$(.RECIPEPREFIX)@echo running $$^...
$(.RECIPEPREFIX)$(a)cp $(d)../../src/out/$(c)/libsvgren$(dot_so) . || true
$(.RECIPEPREFIX)$(a)LD_LIBRARY_PATH=$(d)../../src/out/$(c) $(d)out/$(c)/tests $(d)../samples/testdata/tiger.svg $(d)out.png
endef
$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
