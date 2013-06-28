#! /bin/sh
#
# Turns Makefile.am.in into a Makefile.am which can be processed by
# automake. This is necessary because automake cannot build a list
# of source files dynamically.

# find expression for files which are in sysync_SDK/Sources but only
# need to be compiled into libsynthesissdk.a
SDK_FILES='-name admindata.cpp -o -name admindata.h -o -name blobs.cpp -o -name blobs.h -o -name dbitem.cpp -o -name dbitem.h -o -name enginemodulebridge.cpp -o -name enginemodulebridge.h -o -name stringutil.cpp -o -name stringutil.h -o -name target_options.h -o -name timeutil.cpp -o -name timeutil.h -o -name UI_util.cpp -o -name UI_util.h'

sed -e "s;@LIBSYNTHESIS_SOURCES@;`find sysync DB_interfaces sysync_SDK/Sources Transport_interfaces/engine platform_adapters syncapps/clientEngine_custom \( -name '*.cpp' -o -name '*.[ch]' \) \! \( ${SDK_FILES} -o -name clientprovisioning_inc.cpp -o -name \*_tables_inc.cpp -o -name syncserver.cpp -o -name syncsessiondispatch.cpp \) -printf '%p '`;" \
    -e "s;@LIBSYNTHESISSDK_SOURCES_BOTH@;`find sysync_SDK/Sources \( -name '*.cpp' -o -name '*.c' \) -printf '%p '`;" \
    -e "s;@LIBSYNTHESISSDK_SOURCES_SDK_ONLY@;`find sysync_SDK/Sources \( -name '*.cpp' -o -name '*.c' \) -a \( ${SDK_FILES} \) -printf '%p '`;" \
    -e "s;@LIBSMLTK_SOURCES@;`find syncml_tk \( -name '*.cpp' -o -name '*.[ch]' \) \! \( -wholename syncml_tk/src/sml/\*/palm/\* -o -wholename syncml_tk/src/sml/\*/win/\* \) -printf '%p '`;" \
    -e "s;@LIBSYNTHESISSDK_HEADERS@;`find sysync_SDK/Sources \( -name '*.h' \) -printf 'synthesis/%f '`;" \
    Makefile.am.in >Makefile.am
