AC_PREREQ([2.69])
AC_INIT([CEL], [D.1.0], [felicity@loreley.flyingparchment.org.uk])
AC_CONFIG_SRCDIR([celcore/tokens.c])
AC_CONFIG_HEADERS([include/celcore/cel-config.h])

AC_CANONICAL_HOST

case $host_os in
	darwin*)
		CPPFLAGS="$CPPFLAGS -D_DARWIN_USE_64_BIT_INODE=1"
		;;
	linux*)
		CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64"
		;;
	netbsd*)
		CPPFLAGS="$CPPFLAGS -D_NETBSD_SOURCE"
		;;
	solaris*)
		CPPFLAGS="-D__EXTENSIONS__ -D_FILE_OFFSET_BITS=64"
		;;
esac

AC_PROG_CC
AC_PROG_INSTALL

AC_CONFIG_FILES([Makefile include/Makefile include/celcore/Makefile celcore/Makefile test/Makefile])
AC_OUTPUT
