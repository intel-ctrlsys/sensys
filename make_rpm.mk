name = orcm
arch = $(shell uname -p)
version = 0.0.0
#$(shell git describe | sed 's|[^0-9]*\([0-9]*\.[0-9]*\.[0-9]*\).*|\1|')
release = 1
#$(shell git describe | sed 's|[^0-9]*[0-9]*\.[0-9]*\.[0-9]*-\([0-9]*\).*|\1|')

src  = $(shell cat MANIFEST)

topdir = $(HOME)/rpmbuild
rpm = $(topdir)/RPMS/$(arch)/$(name)-$(version)-$(release).$(arch).rpm
srpm = $(topdir)/SRPMS/$(name)-$(version)-$(release).src.rpm
specfile = $(topdir)/SPECS/$(name)-$(version).spec
source_tar = $(topdir)/SOURCES/$(name)-$(version).tar.gz

rpmbuild_flags = -E '%define _topdir $(topdir)' -E '%define configure_flags $(configure_flags)'
rpmclean_flags = $(rpmbuild_flags) --clean --rmsource --rmspec

configure_flags = --enable-autogen --with-orcm-prefix=orcm_ 

include make.spec

all: $(rpm)

$(rpm): $(manifest) $(specfile) $(source_tar)
	rpmbuild $(rpmbuild_flags) $(specfile) -ba

$(manifest): MANIFEST 
	find *.* > $@

$(source_tar): $(topdir) $(specfile) 
	if [ -n "$(revision)" ]; then \
	  git archive $(revision) -o $@; \
	else \
	  tar -c -z -v -T MANIFEST -f $@; \
	fi ; \
	rpmbuild $(rpmbuild_flags) $(specfile) -bp

$(specfile): $(topdir) make.spec
	@echo "$$make_spec" > $@

$(topdir):
	mkdir -p $@/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

clean:
	-rpmbuild $(rpmclean_flags) $(specfile)

.PHONY: all clean
