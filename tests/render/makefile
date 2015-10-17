include prorab.mk

this_name := tests

this_cflags += -Wall
this_cflags += -Wno-format #no warnings about format
this_cflags += -DDEBUG
this_cflags += -fstrict-aliasing #strict aliasing!!!
this_cflags += -std=c++11

this_srcs += main.cpp

this_ldlibs += -lsvgdom -lsvgren -lpapki -lpugixml -lX11
this_ldflags += -L$(prorab_this_dir)../../src

#TODO: remove when stabilized
this_cflags += -I$(prorab_this_dir)../../../svgdom/src
this_ldflags += -L$(prorab_this_dir)../../../svgdom/src

ifeq ($(prorab_os), macosx)
    this_cflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(prorab_os),windows)
    this_cflags += -I/usr/include -I/usr/local/include
    this_ldflags += -L/usr/lib -L/usr/local/lib
endif


$(eval $(prorab-build-app))


ifeq ($(prorab_os),windows)
    this_test_cmd := $(prorab_echo)(cd $(prorab_this_dir); cp ../../src/libsvgren.dll .; $$^)
else
    this_test_cmd := $(prorab_echo)(cd $(prorab_this_dir); LD_LIBRARY_PATH=../../src $$^)
endif


define this_rules
test:: $(prorab_this_name)
	@echo running $$^...
	$(this_test_cmd)
endef
$(eval $(this_rules))

#add dependency on libsvgren
$(prorab_this_name): $(abspath $(prorab_this_dir)../../src/libsvgren$(prorab_lib_extension))

$(eval $(call prorab-include,$(prorab_this_dir)../../src/makefile))

#add dependency on libsvgdom
$(prorab_this_name): $(abspath $(prorab_this_dir)../../../svgdom/src/libsvgdom$(prorab_lib_extension))

$(eval $(call prorab-include,$(prorab_this_dir)../../../svgdom/src/makefile))
