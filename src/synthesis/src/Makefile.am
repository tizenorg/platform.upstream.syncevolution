# The Makefile.am is generated from Makefile.am.in by autogen.sh
# because the list of files to compile has to be determined
# dynamically.
#
# When adding new files or changing options rerun autogen.sh.
# Beware that changes made to Makefile.am will get lost, always
# edit Makefile.am.in! The following rule ensures that make
# itself will rebuild its makefiles when Makefile.am.in is edited.
$(srcdir)/Makefile.am: $(srcdir)/Makefile.am.in $(srcdir)/gen-makefile-am.sh
	cd $(srcdir) && sh ./gen-makefile-am.sh

CLEANFILES =
EXTRA_DIST =

lib_LTLIBRARIES = libsmltk.la libsynthesis.la libsynthesissdk.la
nobase_include_HEADERS = synthesis/dataconversion.h synthesis/sync_dbapi.h synthesis/sync_include.h synthesis/UI_util.h synthesis/enginemodulebridge.h synthesis/sync_uiapi.h synthesis/enginemodulebase.h synthesis/sync_dbapidef.h synthesis/stringutil.h synthesis/engine_defs.h synthesis/blobs.h synthesis/timeutil.h synthesis/prefix_file.h synthesis/dbitem.h synthesis/admindata.h synthesis/SDK_util.h synthesis/generic_types.h synthesis/syerror.h synthesis/SDK_support.h synthesis/sync_declarations.h  synthesis/target_options.h

# Configuration header files are searched in this order:
# - Targets/ReleasedProducts/autotools: files only used when compiling with autotools
# - Targets/ReleasedProducts/[clientEngine_opensource_linux|SDK]: files also used with Synthesis Linux makefile
#
# Always included first via -include:
# - config.h: configure result (top directory)
# - Targets/ReleasedProducts/clientEngine_opensource_linux/clientengine_demo_x86_linux_prefix.h
#
# Included as needed:
# - define.h: hard-coded SyncML Toolkit config (from search path above)

libsynthesis_la_SOURCES = sysync/tz_table.h sysync/syncdatastore.cpp sysync/vtimezone.h sysync/dataobjtype.h sysync/scriptcontext.h sysync/rrules.cpp sysync/engineentry.cpp sysync/engineentry.h sysync/smltk_precomp_xpt.h sysync/mimedirprofile.cpp sysync/syncclientbase.h sysync/configelement.cpp sysync/lineartime.cpp sysync/uiapi.h sysync/sysync_b64.cpp sysync/multifielditemtype.h sysync/dataconversion.cpp sysync/syncsession.h sysync/stdlogicagent.cpp sysync/sysync_b64.h sysync/synccommand.h sysync/sysync_utils.cpp sysync/superdatastore.cpp sysync/configelement.h sysync/stringutils.cpp sysync/debuglogger.h sysync/sysync_md5.cpp sysync/sysync_debug.h sysync/multifielditem.h sysync/binfilebase.cpp sysync/remotedatastore.cpp sysync/remotedatastore.h sysync/stringutils.h sysync/clientautosync_inc.h sysync/binfilebase.h sysync/syncitem.h sysync/multifielditem.cpp sysync/syncserver.h sysync/textprofile.cpp sysync/sysync_precomp_xpt.h sysync/syncappbase.cpp sysync/customimplds.cpp sysync/debuglogger.cpp sysync/syncclientbase.cpp sysync/sysync_crc16.cpp sysync/vcalendaritemtype.h sysync/syncappbase.h sysync/sysync.h sysync/mimedirprofile.h sysync/customimplagent.h sysync/sysync_utils.h sysync/vcalendaritemtype.cpp sysync/customimplagent.cpp sysync/mimediritemtype.h sysync/synccommand.cpp sysync/engineinterface.cpp sysync/syncdatastore.h sysync/smltk_precomp.h sysync/sysync_md5.h sysync/syncml_globs.h sysync/binfileimplclient.cpp sysync/syncclient.h sysync/syserial.h sysync/syncexception.cpp sysync/customimplds.h sysync/syncsessiondispatch.h sysync/syncsession.cpp sysync/syncitemtype.h sysync/localengineds.h sysync/stdlogicds.cpp sysync/stdlogicds.h sysync/textprofile.h sysync/stdlogicagent.h sysync/localengineds.cpp sysync/itemfield.h sysync/binfileimplds.cpp sysync/syncml_tk.h sysync/textitemtype.cpp sysync/sysync_globs.h sysync/sysync_precomp.h sysync/scriptcontext.cpp sysync/vcarditemtype.cpp sysync/textitemtype.h sysync/engineinterface.h sysync/multifielditemtype.cpp sysync/syncitemtype.cpp sysync/syncclient.cpp sysync/simpleitem.h sysync/vtimezone.cpp sysync/vcarditemtype.h sysync/sysync_crc16.h sysync/timezones.cpp sysync/lineartime.h sysync/simpleitem.cpp sysync/clientprovisioning_inc.h sysync/san.h sysync/syncexception.h sysync/clientautosync_inc.cpp sysync/rrules.h sysync/syncitem.cpp sysync/timezones.h sysync/binfileimplclient.h sysync/binfileimplds.h sysync/mimediritemtype.cpp sysync/iso8601.h sysync/itemfield.cpp sysync/dataobjtype.cpp sysync/superdatastore.h sysync/iso8601.cpp sysync/global_progress.h sysync/uiapi.cpp sysync/san.cpp DB_interfaces/odbc_db/odbcapids.cpp DB_interfaces/odbc_db/odbcdb.h DB_interfaces/odbc_db/odbcapiagent.h DB_interfaces/odbc_db/odbcdb_precomp.h DB_interfaces/odbc_db/odbcapids.h DB_interfaces/odbc_db/odbcapiagent.cpp DB_interfaces/api_db/sync_dbapiconnect.h DB_interfaces/api_db/DLL_interface.h DB_interfaces/api_db/sync_dbapiconnect.cpp DB_interfaces/api_db/pluginapiagent.h DB_interfaces/api_db/DLL_interface.cpp DB_interfaces/api_db/pluginapids.cpp DB_interfaces/api_db/plugindb.h DB_interfaces/api_db/dbapi_include.h DB_interfaces/api_db/dbapi.cpp DB_interfaces/api_db/dbapi.h DB_interfaces/api_db/pluginapids.h DB_interfaces/api_db/pluginapiagent.cpp DB_interfaces/api_db/plugindb_precomp.h sysync_SDK/Sources/dataconversion.h sysync_SDK/Sources/sync_dbapi.h sysync_SDK/Sources/sync_include.h sysync_SDK/Sources/SDK_util.c sysync_SDK/Sources/sync_uiapi.h sysync_SDK/Sources/enginemodulebase.h sysync_SDK/Sources/sync_dbapidef.h sysync_SDK/Sources/engine_defs.h sysync_SDK/Sources/SDK_support.cpp sysync_SDK/Sources/enginemodulebase.cpp sysync_SDK/Sources/prefix_file.h sysync_SDK/Sources/SDK_util.h sysync_SDK/Sources/generic_types.h sysync_SDK/Sources/syerror.h sysync_SDK/Sources/SDK_support.h sysync_SDK/Sources/sync_declarations.h Transport_interfaces/engine/engine_client_precomp.h Transport_interfaces/engine/engine_client.h Transport_interfaces/engine/engineclientbase.cpp Transport_interfaces/engine/engineclientbase.h platform_adapters/sysyncinit.cpp platform_adapters/platform_headers.h platform_adapters/platform_thread.h platform_adapters/platform_file.h platform_adapters/sysync_glob_vars.h platform_adapters/binfile.h platform_adapters/profiling.h platform_adapters/unix_common/platform_thread.cpp platform_adapters/unix_common/platform_pipe.h platform_adapters/unix_common/platform_file.cpp platform_adapters/unix_common/platform_mutex.cpp platform_adapters/unix_common/platform_pipe.c platform_adapters/configfiles.h platform_adapters/binfile.cpp platform_adapters/linux/platform_headers.h platform_adapters/linux/configfiles.cpp platform_adapters/linux/platform_DLL.cpp platform_adapters/linux/platform_time.cpp platform_adapters/linux/platform_exec.c platform_adapters/linux/platform_time.h platform_adapters/linux/platform_timezones.cpp platform_adapters/linux/platform_exec.h platform_adapters/linux/profiling.cpp platform_adapters/platform_DLL.h platform_adapters/platform_mutex.h syncapps/clientEngine_custom/product_options.h syncapps/clientEngine_custom/clientengine_custom_precomp.h syncapps/clientEngine_custom/clientengine_custom_Base.h syncapps/clientEngine_custom/clientengine_custom_Base.cpp syncapps/clientEngine_custom/clientengine_custom.h 
libsynthesis_la_CPPFLAGS = -I$(srcdir)/sysync_SDK/Sources
LIBSYNTHESIS_CFLAGS = \
	-include $(top_builddir)/config.h \
	-include $(srcdir)/Targets/ReleasedProducts/clientEngine_opensource_linux/clientengine_demo_x86_linux_prefix.h \
	-I$(srcdir)/Targets/ReleasedProducts/clientEngine_autotools/ \
	-I$(srcdir)/Targets/ReleasedProducts/clientEngine_opensource_linux/ \
	-I$(srcdir)/platform_adapters/linux/ \
	-I$(srcdir)/platform_adapters/unix_common/ \
	-I$(srcdir)/platform_adapters/ \
	-I$(srcdir)/syncapps/clientEngine_custom/ \
	-I$(srcdir)/sysync/ \
	-I$(srcdir)/Transport_interfaces/engine/ \
	-I$(srcdir)/ \
	-I$(srcdir)/syncml_tk/src/sml/inc/ \
	-I$(srcdir)/syncml_tk/src/sml/lib/inc/ \
	-I$(srcdir)/syncml_tk/src/sml/lib/ \
	-I$(srcdir)/syncml_tk/src/sml/mgr/inc/ \
	-I$(srcdir)/syncml_tk/src/sml/mgr/ \
	-I$(srcdir)/syncml_tk/src/sml/wsm/inc/ \
	-I$(srcdir)/syncml_tk/src/sml/xlt/inc \
	-I$(srcdir)/syncml_tk/src/sml/xlt/all \
	-I$(srcdir)/DB_interfaces/odbc_db/ \
	-I$(srcdir)/DB_interfaces/api_db/ \
	-I$(srcdir)/sysync_SDK/Sources/ \
	$(PCRE_CFLAGS) $(SQLITE3_CFLAGS) $(ZLIB_CFLAGS) $(XMLPARSE_CFLAGS) $(LIBICAL_CFLAGS) $(LIBECAL_CFLAGS)
libsynthesis_la_CFLAGS = $(LIBSYNTHESIS_CFLAGS)
libsynthesis_la_CXXFLAGS = $(LIBSYNTHESIS_CFLAGS)
libsynthesis_la_LIBADD = $(PCRE_LIBS) $(SQLITE3_LIBS) $(XMLPARSE_LIBS) $(LIBICAL_LIBS) $(LIBECAL_LIBS) libsmltk.la
if COND_XMLPARSE
LIBSYNTHESIS_CFLAGS += -I$(srcdir)/expat -I$(srcdir)/expat/xmltok -I$(srcdir)/expat/xmlparse
libsynthesis_la_SOURCES += \
	expat/xmlparse/xmlparse.c \
	expat/xmlparse/xmlparse.h \
	expat/xmltok/ascii.h \
	expat/xmltok/asciitab.h \
	expat/xmltok/iasciitab.h \
	expat/xmltok/latin1tab.h \
	expat/xmltok/nametab.h \
	expat/xmltok/utf8tab.h \
	expat/xmltok/xmldef.h \
	expat/xmltok/xmlrole.c \
	expat/xmltok/xmlrole.h \
	expat/xmltok/xmltok.c \
	expat/xmltok/xmltok.h \
	expat/xmltok/xmltok_impl.h
EXTRA_DIST += \
	expat/xmltok/xmltok_impl.c \
	expat/xmltok/xmltok_ns.c
else
# add path to xmlparse.h which includes the system's xmltok/xmlparse.h
LIBSYNTHESIS_CFLAGS += -I$(srcdir)/Targets/ReleasedProducts/clientEngine_autotools/systemxml
endif

# versioning:
# CURRENT - most recent interface version
# REVISION - minor version number of that interface implementation
# AGE - number of previous interface versions supported in addition
#       to the current one
#
# When adding to the interface, increment CURRENT and AGE and reset
# REVISION to zero. When breaking the interface in a backwards
# incompatible way, increment CURRENT and reset AGE and REVISION to
# zero. When fixing something without interface change, increment
# REVISION.
ENGINE_CURRENT = 0
ENGINE_REVISION = 0
ENGINE_AGE = 0
libsynthesis_la_LDFLAGS = -version-info $(ENGINE_CURRENT):$(ENGINE_REVISION):$(ENGINE_AGE) \
	-Wl,--version-script=$(srcdir)/synthesis-linker.map
libsynthesis_la_DEPENDENCIES = $(srcdir)/synthesis-linker.map libsmltk.la

libsynthesissdk_la_LIBADD = libsynthesis.la
libsynthesissdk_la_LDFLAGS = -static
libsynthesissdk_la_SOURCES = synthesis/dataconversion.h synthesis/sync_dbapi.h synthesis/sync_include.h synthesis/UI_util.h synthesis/enginemodulebridge.h synthesis/sync_uiapi.h synthesis/enginemodulebase.h synthesis/sync_dbapidef.h synthesis/stringutil.h synthesis/engine_defs.h synthesis/blobs.h synthesis/timeutil.h synthesis/prefix_file.h synthesis/dbitem.h synthesis/admindata.h synthesis/SDK_util.h synthesis/generic_types.h synthesis/syerror.h synthesis/SDK_support.h synthesis/sync_declarations.h 
if COND_STATIC
libsynthesissdk_la_SOURCES += sysync_SDK/Sources/admindata.cpp sysync_SDK/Sources/UI_util.cpp sysync_SDK/Sources/dbitem.cpp sysync_SDK/Sources/stringutil.cpp sysync_SDK/Sources/timeutil.cpp sysync_SDK/Sources/enginemodulebridge.cpp sysync_SDK/Sources/blobs.cpp 
else
libsynthesissdk_la_SOURCES += sysync_SDK/Sources/SDK_util.c sysync_SDK/Sources/admindata.cpp sysync_SDK/Sources/UI_util.cpp sysync_SDK/Sources/dbitem.cpp sysync_SDK/Sources/SDK_support.cpp sysync_SDK/Sources/stringutil.cpp sysync_SDK/Sources/enginemodulebase.cpp sysync_SDK/Sources/timeutil.cpp sysync_SDK/Sources/enginemodulebridge.cpp sysync_SDK/Sources/blobs.cpp 
endif
libsynthesissdk_la_CPPFLAGS = \
	-I$(srcdir)/sysync_SDK/Sources \
	-I$(srcdir)/Targets/ReleasedProducts/SDK

# All files needed by libsynthesissdk.a come from either
# sysync_SDK/Sources (shared between engine and SDK) or
# sysync_SDK/SDK (only in SDK). We patch the files so that
# they include their own files via #include "synthesis/..."
vpath %.cpp $(srcdir)/sysync_SDK/Sources $(srcdir)/sysync_SDK/SDK # $(srcdir)/Targets/ReleasedProducts/clientEngine_opensource_linux $(srcdir)/syncapps/clientEngine_custom $(srcdir)/sysync $(srcdir)/platform_adapters $(srcdir)
vpath %.c $(srcdir)/sysync_SDK/Sources $(srcdir)/sysync_SDK/SDK # $(srcdir)/Targets/ReleasedProducts/clientEngine_opensource_linux $(srcdir)/syncapps/clientEngine_custom $(srcdir)/sysync $(srcdir)/platform_adapters $(srcdir)
vpath %.h $(srcdir)/sysync_SDK/Sources $(srcdir)/sysync_SDK/SDK # $(srcdir)/Targets/ReleasedProducts/clientEngine_opensource_linux $(srcdir)/syncapps/clientEngine_custom $(srcdir)/sysync $(srcdir)/platform_adapters $(srcdir)
synthesis/dataconversion.h synthesis/sync_dbapi.h synthesis/sync_include.h synthesis/UI_util.h synthesis/enginemodulebridge.h synthesis/sync_uiapi.h synthesis/enginemodulebase.h synthesis/sync_dbapidef.h synthesis/stringutil.h synthesis/engine_defs.h synthesis/blobs.h synthesis/timeutil.h synthesis/prefix_file.h synthesis/dbitem.h synthesis/admindata.h synthesis/SDK_util.h synthesis/generic_types.h synthesis/syerror.h synthesis/SDK_support.h synthesis/sync_declarations.h : synthesis/%: %
	mkdir -p synthesis
	sed -e 's;# *include  *";#include "synthesis/;' -e 's/defined(HAVE_STDINT_H)/1/' $< >$@

synthesis/target_options.h: $(srcdir)/Targets/ReleasedProducts/SDK/target_options.h
	mkdir -p synthesis
	cp $< $@

# ensure that the header file is in place before compiling the SDK
@LIBSYNTHESISSDK_SOURCES@: synthesis/target_options.h

# libsmltk: compiled just like libsynthesis
libsmltk_la_SOURCES = syncml_tk/src/sml/lib/all/liblock.c syncml_tk/src/sml/lib/all/libutil.c syncml_tk/src/sml/lib/all/libstr.c syncml_tk/src/sml/lib/all/libmem.c syncml_tk/src/sml/lib/inc/libstr.h syncml_tk/src/sml/lib/inc/libmem.h syncml_tk/src/sml/lib/inc/liblock.h syncml_tk/src/sml/lib/inc/libutil.h syncml_tk/src/sml/mgr/all/mgrinstancelist.c syncml_tk/src/sml/mgr/all/mgr.c syncml_tk/src/sml/mgr/all/mgrcmdbuilder.c syncml_tk/src/sml/mgr/all/mgrcmddispatcher.c syncml_tk/src/sml/mgr/all/mgrutil.c syncml_tk/src/sml/mgr/all/mgrinstancemgr.c syncml_tk/src/sml/mgr/inc/mgr.h syncml_tk/src/sml/mgr/inc/mgrutil.h syncml_tk/src/sml/xlt/all/xltdeccom.h syncml_tk/src/sml/xlt/all/xltdecwbxml.c syncml_tk/src/sml/xlt/all/xltdecxml.c syncml_tk/src/sml/xlt/all/xltdec.c syncml_tk/src/sml/xlt/all/xltdevinf.c syncml_tk/src/sml/xlt/all/xltmetinf.h syncml_tk/src/sml/xlt/all/xlttags.c syncml_tk/src/sml/xlt/all/xltencwbxml.h syncml_tk/src/sml/xlt/all/xltencwbxml.c syncml_tk/src/sml/xlt/all/xltenc.c syncml_tk/src/sml/xlt/all/xltmetinf.c syncml_tk/src/sml/xlt/all/xltencxml.c syncml_tk/src/sml/xlt/all/xltutilstack.c syncml_tk/src/sml/xlt/all/xltutilstack.h syncml_tk/src/sml/xlt/all/xltdecwbxml.h syncml_tk/src/sml/xlt/all/xlttags.h syncml_tk/src/sml/xlt/all/xltenccom.h syncml_tk/src/sml/xlt/all/xltencxml.h syncml_tk/src/sml/xlt/all/xltdevinf.h syncml_tk/src/sml/xlt/all/xltenccom.c syncml_tk/src/sml/xlt/all/xlttagtbl.h syncml_tk/src/sml/xlt/inc/xltenc.h syncml_tk/src/sml/xlt/inc/xltdec.h syncml_tk/src/sml/wsm/all/wsm.c syncml_tk/src/sml/wsm/inc/wsm.h syncml_tk/src/sml/wsm/inc/wsm_sm.h syncml_tk/src/sml/inc/smldtd.h syncml_tk/src/sml/inc/smldevinfdtd.h syncml_tk/src/sml/inc/smlmetinfdtd.h syncml_tk/src/sml/inc/sml.h syncml_tk/src/sml/inc/smldef.h syncml_tk/src/sml/inc/smlerr.h 
libsmltk_la_CFLAGS = $(LIBSYNTHESIS_CFLAGS)
libsmltk_la_CXXFLAGS = $(LIBSYNTHESIS_CFLAGS)

# versioning: same as of engine! changes in libsmltk are not tracked separately.
libsmltk_la_LDFLAGS = -version-info $(ENGINE_CURRENT):$(ENGINE_REVISION):$(ENGINE_AGE) \
	-Wl,--version-script=$(srcdir)/smltk-linker.map
libsmltk_la_DEPENDENCIES = $(srcdir)/smltk-linker.map
