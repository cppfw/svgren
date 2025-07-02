include prorab.mk
include prorab-license.mk
include prorab-clang-format.mk
include prorab-install-dbgsrc.mk

$(eval $(call prorab-config, ../config))

this_src_dir := svgren

this_name := $(this_src_dir)$(this_dbg)

this_soname := $(shell cat $(d)soname.txt)

this_srcs := $(call prorab-src-dir, $(this_src_dir))

# this_cxxflags += -D SVGREN_BACKGROUND=0xffffffff
# this_cxxflags += -D SVGREN_BACKGROUND=0xff00ffff

this_ldlibs += -l veg$(this_dbg)
this_ldlibs += -l svgdom$(this_dbg)
this_ldlibs += -l utki$(this_dbg)

this_ldlibs += -l m

$(eval $(prorab-build-lib))

this_license_file := ../LICENSE
$(eval $(prorab-license))

$(eval $(prorab-clang-format))

$(eval $(prorab-install-dbgsrc))
