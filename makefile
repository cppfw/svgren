include prorab.mk


$(eval $(prorab-build-subdirs))


#install pkg-config files
install::
	@install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	@install pkg-config/*.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig



$(prorab-clear-this-vars)

this_soname_dependency := $(prorab_this_dir)src/soname.txt

this_soname := $(shell cat $(this_soname_dependency))

$(eval $(prorab-build-deb))


#Update version rule
$(prorab-clear-this-vars)

this_version_files += doc/doxygen.cfg.in
this_version_files += pkg-config/svgren.pc.in
this_version_files += nuget.autopkg.in

$(eval $(prorab-apply-version))
