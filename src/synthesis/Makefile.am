## Process this file with automake to produce Makefile.in

SUBDIRS = src

# list all additional files which need to be included by "make dist",
# check with "make distcheck"
EXTRA_DIST =
# extra files which need to be removed during "make distclean"
MAINTAINERCLEANFILES = Makefile.in config.guess config.sub configure depcomp install-sh ltmain.sh missing mkinstalldirs

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = synthesis.pc
