include prorab.mk

this_name := checksvg

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format
this_cxxflags += -Wno-format-security
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing
this_cxxflags += -std=c++14

this_srcs += $(call prorab-src-dir, .)

this_ldlibs += -lsvgdom $(d)../../src/build/libsvgren$(dot_so) -lpapki -lclargs `pkg-config --libs cairo` -lpng -lstdc++

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ # this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
endif

this_no_install := true

$(eval $(prorab-build-app))

this_svgs := $(patsubst %.svg,%,$(shell ls $(d)testdata/*.svg))

this_dirs := $(subst /, ,$(d))
this_test := $(word $(words $(this_dirs)),$(this_dirs))

this_arch_bit_depth := $(shell getconf LONG_BIT)

# on 32 bit systems the actual rendered result differs a bit from PNG images generated on 64 bit system,
# so do this test only for 64 bit systems
ifeq ($(this_arch_bit_depth),64)
define this_rules
test:: $(prorab_this_name)
$(.RECIPEPREFIX)@myci-running-test.sh $(this_test)
$(.RECIPEPREFIX)$(a)cp $(d)../../src/build/*.dll . || true
$(.RECIPEPREFIX)$(a)for f in $(this_svgs); do \
        echo "checking file $$$$f.svg" && \
        LD_LIBRARY_PATH=$(d)../../src/build $(d)./checksvg --tolerance=2 $$$$f.svg $$$$f.png; \
        if [ $$$$? -ne 0 ]; then myci-error.sh "rendered SVG did not match etalon sample"; exit 1; fi \
    done
$(.RECIPEPREFIX)@myci-passed.sh
endef
$(eval $(this_rules))
endif

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/build/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
