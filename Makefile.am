## Spindle: The RES Linked Open Data Aggregation Engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2018 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

ACLOCAL_AMFLAGS = -I m4

DIST_SUBDIRS = m4 common strip correlate generate docbook-html5 docs

SUBDIRS = common strip correlate generate migrate docs

EXTRA_DIST = LICENSE-2.0 README.md old-README.md

BRANCH ?= develop
DOCS_BRANCH ?= gh-pages

DEVELOP_SUBMODULES = m4
DOCS_SUBMODULES = docbook-html5 docs

checkout:
	@for submod in $(DEVELOP_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(BRANCH) && \
		git pull && \
		$(MAKE) checkout BRANCH=$(BRANCH) || exit $? && \
		cd .. ; \
	done
	@for submod in $(DOCS_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(DOCS_BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(DOCS_BRANCH) && \
		git pull && \
		cd .. ; \
	done

reconf:
	(cd $(top_srcdir) && autoreconf -i ) && $(SHELL) $(top_builddir)/config.status --recheck && $(SHELL) $(top_builddir)/config.status
