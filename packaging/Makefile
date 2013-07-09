PKGNAME = syncevolution

all: ${PKGNAME}.spec

${PKGNAME}.spec: ${PKGNAME}.yaml
	specify

clean:
	rm -f ${PKGNAME}.spec

build: ${PKGNAME}.spec
	cd .. && gbs --verbose build  -A i586 --clean --include-all

quickbuild: ${PKGNAME}.spec
	cd .. && gbs --verbose build  -A i586 --include-all
