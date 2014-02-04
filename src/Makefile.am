#
# replace 'plugin' on the right side by your plugin's name
#
plugin_LTLIBRARIES = libdlvhexplugin-nestedhex.la

#
# replace 'plugin' on the left side as above and
# add all sources of your plugin
#
libdlvhexplugin_nestedhex_la_SOURCES = NestedHexPlugin.cpp

#
# extend compiler flags by CFLAGS of other needed libraries
#
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(BOOST_CPPFLAGS) \
	$(DLVHEX_CFLAGS) \
	$(EXTSOLVER_CPPFLAGS)

libdlvhexplugin_nestedhex_la_LDFLAGS = -avoid-version -module $(EXTSOLVER_LDFLAGS)

libdlvhexplugin_nestedhex_la_LIBADD = $(EXTSOLVER_LIBADD)


libdlvhexplugin-nestedhex-static.la: $(libdlvhexplugin_nestedhex_la_OBJECTS)
	$(CXXLINK) -avoid-version -module -rpath $(plugindir) $(libdlvhexplugin_nestedhex_la_OBJECTS)

install-static: libdlvhexplugin-nestedhex-static.la
	$(LIBTOOL) --mode=install $(INSTALL) -s libdlvhexplugin-nestedhex-static.la $(DESTDIR)$(plugindir)/libdlvhexplugin-nestedhex-static.la

uninstall-static:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(plugindir)/libdlvhexplugin-nestedhex-static.la
