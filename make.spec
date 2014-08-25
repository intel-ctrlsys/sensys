define make_spec

Summary: Open Resilency Cluster Management implementation.
Name: orcm
Version: $(version)
Release: $(release)
License: See COPYING
Group: System Environment/Libraries
Vendor: Intel Corporation
URL: https://bitbucket.org/rhc/orcm/
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Prefix: /usr

%{!?configure_flags: %define configure_flags ""}

%description
orcm is a opensource resilency cluster management software implementation.

%prep
%setup -q -c -T -a 0

%build
mkdir -p obj
cd obj
pwd
../configure %{configure_flags} --prefix=/usr --libdir=/usr/lib --with-platform=../contrib/platform/intel/hillsboro/orcm-linux
$(make_prefix) $(MAKE) $(make_postfix)
#$(MAKE) build_doc

%install
cd obj
make DESTDIR=%{buildroot} install
$(extra_install)

%clean

%files
%defattr(-,root,root,-)
/usr/etc/*
/usr/bin/*
/usr/lib/*
/usr/include/openmpi/*
%doc /usr/share/openmpi/*
%doc /usr/share/man/man1/*
%doc /usr/share/man/man7/*
$(extra_files)

%changelog
* Mon Mar 10 2014 mic <mic@localhost> - 
- Initial build.
endef

export make_spec
