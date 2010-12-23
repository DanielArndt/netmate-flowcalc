DOXY = @DOXY@
DOXYDIR = netmate_source_doc

EXTRA_DIST = FAQ known-bugs wishlist user_manual.pdf Doxyfile.in

# FIXME include doc files in distribution

all:    Doxyfile
        
	@if test $(DOXY) ; then \
	  echo "Building netmate source code documentation (html)" ; \
	  doxygen ; \
	else \
	  echo "WARNING: not building source code documentation because Doxygen is not installed!" ; \
	fi;

clean:
	@echo "Removing netmate source code documentation (html)"
	
	@if test -d $(DOXYDIR) ; then \
	  rm -rf $(DOXYDIR) ; \
	fi;

distclean: clean
	@rm -rf Makefile
	@rm -rf Doxyfile

maintainer-clean: distclean

