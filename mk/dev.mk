DIST_VERSIONS = 1.7 1.6 1.5 1.4 1.3 1.2 1.1 1.0
DIST_ALL = $(addprefix dte-, $(addsuffix .tar.gz, $(DIST_VERSIONS)))
GIT_HOOKS = $(addprefix .git/hooks/, commit-msg pre-commit)

dist: $(firstword $(DIST_ALL))
dist-all: $(DIST_ALL)
git-hooks: $(GIT_HOOKS)

check-dist: dist-all
	@sha256sum -c mk/sha256sums.txt

$(DIST_ALL): dte-%.tar.gz:
	$(E) ARCHIVE $@
	$(Q) git archive --prefix='dte-$*/' -o '$@' 'v$*'

$(GIT_HOOKS): .git/hooks/%: tools/git-hooks/%
	$(E) CP $@
	$(Q) cp $< $@

show-sizes: MAKEFLAGS += \
    -j$(NPROC) --no-print-directory \
    CFLAGS=-O2\ -pipe \
    TERMINFO_DISABLE=1 \
    DEBUG=0 USE_SANITIZER=

show-sizes:
	$(MAKE) USE_LUA=no dte=build/dte-dynamic
	-$(MAKE) USE_LUA=dynamic dte=build/dte-dynamic+lua
	$(MAKE) USE_LUA=static dte=build/dte-dynamic+staticlua
	$(MAKE) USE_LUA=no LDFLAGS=-static dte=build/dte-static
	$(MAKE) USE_LUA=static LDFLAGS=-static dte=build/dte-static+lua
	-$(MAKE) CC=musl-gcc USE_LUA=no LDFLAGS=-static dte=build/dte-musl-static
	-$(MAKE) CC=musl-gcc USE_LUA=static LDFLAGS=-static dte=build/dte-musl-static+lua
	@strip build/dte-*
	@du -h build/dte-*

coverage-report:
	$(MAKE) -j$(NPROC) check CFLAGS='-O2 -g -pipe --coverage' DEBUG=3 USE_SANITIZER=
	lcov -c -b . -d build/ -o build/coverage.info
	genhtml --title 'dte coverage' -o public/coverage/ build/coverage.info
	find public/coverage/ -type f -regex '.*\.\(css\|html\)$$' | \
	  xargs $(XARGS_P_FLAG) -- gzip -9 -k


CLEANFILES += dte-*.tar.gz
.PHONY: dist dist-all check-dist git-hooks show-sizes coverage-report
