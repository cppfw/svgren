include prorab.mk

this_name := checksvg

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format #no warnings about format
this_cxxflags += -Wno-format-security #no warnings about format
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -std=c++11

this_srcs += checksvg.cpp

this_ldlibs += -lsvgdom $(d)../../src/libsvgren$(soext) -lpapki `pkg-config --libs cairo` -lpng -lstdc++

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
endif

this_no_install := true

$(eval $(prorab-build-app))

this_svgs := $(patsubst %.svg,%,$(shell ls $(d)testdata/*.svg))

this_dirs := $(subst /, ,$(d))
this_test := $(word $(words $(this_dirs)),$(this_dirs))

define this_rules
test:: $(prorab_this_name)
	@myci-running-test.sh $(this_test)
	$(prorab_echo)cp $(d)../../src/*.dll . || true
	$(prorab_echo)for f in $(this_svgs); do \
	    echo "checking file $$$$f.svg" && \
	    LD_LIBRARY_PATH=$(d)../../src $(d)./checksvg $$$$f.svg $$$$f.png; \
	    if [ $$$$? -ne 0 ]; then myci-error.sh "rendered SVG did not match etalon sample"; exit 1; fi \
	done
	@myci-passed.sh
endef
$(eval $(this_rules))


#add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/libsvgren$(soext))

$(eval $(call prorab-include,$(d)../../src/makefile))
