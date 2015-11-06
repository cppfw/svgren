include prorab.mk

define this_rule
    install::
    #install pkg-config files
		$(prorab_echo)prorab-apply-version.sh `prorab-deb-version.sh $(prorab_this_dir)../debian/changelog` $(prorab_this_dir)*.pc.in
		$(prorab_echo)install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
		$(prorab_echo)install $(prorab_this_dir)*.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig
endef
$(eval $(this_rule))
