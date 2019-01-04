include prorab.mk

this_name := tests

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format #no warnings about format
this_cxxflags += -Wno-format-security #no warnings about format
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -std=c++11

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/libsvgren$(soext) -lpapki `pkg-config --libs cairo` -lpng -lstdc++ -lpthread

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
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
	@myci-running-test.sh $(this_test)
	$(prorab_echo)cp $(d)../../src/*.dll . || true
	$(prorab_echo)LD_LIBRARY_PATH=$(d)../../src DYLD_LIBRARY_PATH=$$$$LD_LIBRARY_PATH $(d)./tests $(this_svgs); \
		if [ $$$$? -ne 0 ]; then myci-error.sh "test failed"; exit 1; fi
	@myci-passed.sh
endef
#NOTE: this test is turned off because there is some problem with it when running under qemu (raspberry pi) on CI machine
#$(eval $(this_rules))


#add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/libsvgren$(soext))

$(eval $(call prorab-include,$(d)../../src/makefile))
