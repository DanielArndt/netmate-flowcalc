## Makefile.am -- Process this file with automake to produce Makefile.in

SUBDIRS = dist etc src tools doc

PERL = perl


#
# Build source and binary rpms. For rpm-3.0 and above, the ~/.rpmmacros
# must contain the following line:
# %_topdir /home/<user>/rpm
# and that /home/<user>/rpm contains the directory SOURCES, BUILD etc.
#
# cd /home/<user>/rpm ; mkdir -p SOURCES BUILD RPMS/i586 SPECS SRPMS
#
# If additional configure flags are needed to build the package, add the
# following in ~/.rpmmacros
# %configure CFLAGS="%{optflags}" ./configure %{_target_platform} --prefix=%{_prefix} ${AM_CONFIGFLAGS}
# and run make rpms in the following way:
# AM_CONFIGFLAGS='--param=value' make rpms
#

rpmmacros:
	@if test ! -e $(HOME)/.rpmmacros ; then \
          echo "Can't find $(HOME)/.rpmmacros. Read the documentation in Makefile.am." ; \
        fi;

rpms: rpmmacros $(HOME)/.rpmmacros
	$(MAKE) dist
	$(MAKE) RPMDIST=$(PACKAGE) rpm
	$(MAKE) RPMDIST=$(PACKAGE)-ssl rpm

rpm:
	RPM_TOPDIR=`rpm --showrc | $(PERL) -n -e 'print if(s/.*_topdir\s+(.*)/$$1/)'` ; \
	cp dist/$(RPMDIST).spec $$RPM_TOPDIR/SPECS ; \
	cp $(PACKAGE)-$(VERSION).tar.gz $$RPM_TOPDIR/SOURCES/ ; \
	rpmbuild -ba --clean --rmsource $$RPM_TOPDIR/SPECS/$(RPMDIST).spec ; \
	mv $$RPM_TOPDIR/RPMS/i586/$(RPMDIST)-*.rpm . ; \
	mv $$RPM_TOPDIR/SRPMS/$(RPMDIST)-*.src.rpm .

#
# omit stuff that should not be released yet
# -> distribute Makefile.dist (if present) instead of Makefile.am
#

dist-hook:
	@echo "Replacing makefiles for distribution:" ; \
	SFILES=`find $(srcdir) -name 'Makefile.dist'` ; \
	for SFILE in $$SFILES ; do \
	  TFILE=`echo $$SFILE | sed -e 's/\.//g' | sed -e 's/Makefiledist/Makefile.am/g'` ; \
	  echo "  $$TFILE" ; \
	  cp -f $$SFILE $(distdir)/$$TFILE ; \
	done ; \
	cd $(distdir) ; \
	autoheader ; \
	automake ; \
	autoconf ; \
	rm -rf autom4te.cache ; \
	cd $(srcdir) ;

