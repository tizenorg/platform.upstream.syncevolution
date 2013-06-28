#! /bin/sh
#
# This script generates the autotools configure.in and
# Makefile.am files from the information provided by
# SyncEvolution and backends in src/backends. The
# motivation for this non-standard approach was that
# it allows adding new backends without touching core
# files, which should have simplified the development of
# out-of-tree backends. Now git pretty much removes
# the need for such tricks, but it's still around.

# Another reason for gen-autotools.sh is that it generates
# the version in the configure script. This cannot be
# done inside the script because autoconf expects a
# literal string, not some kind of variable.
#
# To use the version specified in AC_INIT() unmodified,
# the following checks must pass:
# - SyncEvolution source is clean (git status reports
#   no "modified" files or "untracked" files, or the source
#   is not in git at all)
# - the source is tagged with the version of SyncEvolution
#   (git describe --tags HEAD reports something which matches,
#   for example syncevolution-1-0-beta-2a for 1.0beta2a)
# - same for libsynthesis, if the SYNTHESISSRC env variable
#   is set
#
# If these tests fail, the version is extended:
# +<yyyymmdd>+SE+<status>+SYSYNC+<status>
# <yyyymmdd> = date
# <status> = <hash>[+unclean]
# <hash> = shortened hash from describe (for example, 1040ffd)
# +unclean = source was dirty

set -e

version=`grep '^AC_INIT' configure-pre.in | sed -e 's/.*\[\(.*\)\])/\1/'`
checksource () {
    dir=$1
    force=$2
    dirty=
    if [ ! -d $dir/.git ]; then
        return
    fi

    cur=`pwd`
    cd $dir
    
    if git status | grep -e "modified:" -e "Untracked files:" -q; then
        dirty=+unclean
    fi
    describe=`git describe --tags`
    hash=`cat .git/HEAD | sed -e 's/ref: //'`
    hash=`git show-ref --abbrev --hash --verify $hash`
    if echo $describe | grep -e '-[0-9]+-[0-9a-f]{8}$' -q; then
        exact=
        tag=`echo $describe | sed -e 's/-[0123456789]*-g.*//'`
    else
        exact=1
        tag=$describe
    fi
    simpletag=$tag
    # Hyphens between numbers in the tag are dots in the version
    # and all other hyphens can be removed.
    while true; do
        tmp=`echo $simpletag | sed -e 's/\([0123456789]\)-\([0123456789]\)/\1.\2/'`
        if [ $tmp = $simpletag ]; then
            break
        else
            simpletag=$tmp
        fi
    done
    simpletag=`echo $simpletag | sed -e 's/-//g'`
    if [ "$dirty" ] || [ "$force" ]; then
        # previous check failed, always print hash
        echo $hash$dirty
    elif [ "$exact" ] &&
        echo $simpletag | grep -q "syncevolution${version}\$"; then
        true
    else
        echo $hash$dirty
    fi
    cd $cur
}

versionsuffix=
syncevo=`checksource .`
if [ "$SYNTHESISSRC" ]; then
    sysync=`checksource $SYNTHESISSRC $syncevo`
fi
# run check again, to get hash when only libsynthesis failed
syncevo=`checksource . $sysync`
if [ "$syncevo" ]; then
    versionsuffix=+SE+$syncevo
fi
if [ "$sysync" ]; then
    versionsuffix=$versionsuffix+SYSYNC+$sysync
fi
if [ "$versionsuffix" ]; then
    versionsuffix=+`date +%Y%m%d`$versionsuffix
fi


# generate configure.in from main configure-*.in pieces
# and all backend configure-sub.in pieces
rm -f configure.in
sed -e "s/^\\(AC_INIT.*\\)\\[\\(.*\\)\\]/\\1[\\2$versionsuffix]/" configure-pre.in >>configure.in

BACKENDS=
SUBS=
for sub in src/backends/*/configure-sub.in; do
    BACKENDS="$BACKENDS `dirname $sub | sed -e 's;^src/;;'`"
    SUBS="$SUBS $sub"
    echo "# vvvvvvvvvvvvvv $sub vvvvvvvvvvvvvv" >>configure.in
    cat $sub >>configure.in
    echo "AC_CONFIG_FILES(`echo $sub | sed -e s/configure-sub.in/Makefile/`)" >>configure.in
    echo "# ^^^^^^^^^^^^^^ $sub ^^^^^^^^^^^^^^" >>configure.in
    echo >>configure.in
done
cat configure-post.in >>configure.in

TEMPLATE_FILES=`cd src && find templates -type f \( -name README -o -name '*.png' -o -name '*.svg' -o -name '*.ini' \) | sort`
TEMPLATE_FILES=`echo $TEMPLATE_FILES`

# create Makefile.am files
sed -e "s;@BACKEND_REGISTRIES@;`echo src/backends/*/*Register.cpp | sed -e s%src/%%g`;" \
    -e "s;@BACKENDS@;$BACKENDS;" \
    -e "s;@TEMPLATE_FILES@;$TEMPLATE_FILES;" \
     src/Makefile-gen.am >src/Makefile.am

sed -e "s;@CONFIG_SUBS@;$SUBS;" \
    Makefile-gen.am >Makefile.am

# create LINGUAS file: every .po is included
(cd po && ls -1 *.po | sort -u | sed -e 's/.po$//' > LINGUAS)
