## Makefile.am -- Process this file with automake to produce Makefile.in

nodist_bin_SCRIPTS = nmctl 

dist_sysconf_DATA = netmate.conf.dtd filterdef.xml \
                    filterdef.dtd filterval.xml filterval.dtd rulefile.dtd \
                    main.html reply.xml reply.xsl reply2.xsl \
		    example_rules1.xml example_rules2.xml example_rules3.xml \
		    #ipfix_mapping.txt
nodist_sysconf_DATA = netmate.conf.xml

DISTCLEANFILES = nmctl gen_cert.sh netmate.conf.xml

if ENABLE_SSL
  nodist_bin_SCRIPTS += gen_cert.sh 
  DISTCLEANFILES += netmate.pem
endif

install-exec-hook: 
if ENABLE_SSL
	@echo "Generating certificate for SSL" ; \
	$(DESTDIR)/$(bindir)/gen_cert.sh $(DESTDIR)/$(sysconfdir)
endif
