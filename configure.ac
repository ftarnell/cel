AC_PREREQ([2.69])
AC_INIT([CEL], [D.1.0], [felicity@loreley.flyingparchment.org.uk])
AC_CONFIG_SRCDIR([celcore/tokens.c])
AC_CONFIG_HEADERS([build.h])

AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(CEL_HOST, ["$host"], [Host triplet])

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

AC_CHECK_HEADERS([histedit.h],[
	AC_CHECK_LIB([edit], [el_init],
		[AC_DEFINE([HAVE_LIBEDIT], [1], [Define if libedit is available])
		 EDIT_LIBS=-ledit
		 AC_SUBST(EDIT_LIBS)])])

AC_CONFIG_FILES([Makefile include/Makefile include/celcore/Makefile celcore/Makefile celvm/Makefile test/Makefile icel/Makefile])
AC_CONFIG_COMMANDS([cel], [
mkdir include 2>/dev/null || true
mkdir include/celcore 2>/dev/null || true
cat >include/celcore/cel-config.h.tmp <<EOF
#ifndef	CEL_CONFIG_H
#define	CEL_CONFIG_H

#define	CEL_VERSION "$PACKAGE_VERSION"
#define	CEL_HOST "$host"

#endif	/* !CEL_CONFIG_H */
EOF

if ! cmp include/celcore/cel-config.h.tmp include/celcore/cel-config.h >/dev/null 2>&1; then
	mv include/celcore/cel-config.h.tmp include/celcore/cel-config.h
else
	echo config.status: include/celcore/cel-config.h is unchanged
	rm include/celcore/cel-config.h.tmp
fi

],
[host="$host"; PACKAGE_VERSION="$PACKAGE_VERSION"])

AC_OUTPUT
