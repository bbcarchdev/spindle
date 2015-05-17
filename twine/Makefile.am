## Spindle: The RES Linked Open Data Aggregation Engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2015 BBC
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

EXTRA_DIST = README.md

AM_CPPFLAGS = @AM_CPPFLAGS@ @LIBTWINE_CPPFLAGS@ \
	-I$(top_builddir)/libs3client -I$(top_srcdir)/libs3client

twinemodule_LTLIBRARIES = spindle.la

spindle_la_SOURCES = p_spindle.h \
	module.c preproc.c postproc.c proxy.c refset.c cache.c classes.c props.c \
	strset.c rulebase.c doc.c licenses.c db.c schema.c

spindle_la_LDFLAGS = -no-undefined -module -avoid-version

spindle_la_LIBADD = $(top_builddir)/libs3client/libs3client.la \
	@LIBTWINE_LOCAL_LIBS@ @LIBTWINE_LIBS@

dist_twinemodule_DATA = rulebase.ttl
