## Spindle: The RES Linked Open Data Aggregation Engine
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2016 BBC
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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	@LIBMQ_CPPFLAGS@ \
	@LIBRULEBASE_CPPFLAGS@ \
	@LIBTWINE_CPPFLAGS@ \
	-I$(srcdir)/../common

twinemodule_LTLIBRARIES = spindle-generate.la

spindle_generate_la_SOURCES = p_spindle-generate.h \
	module.c processor.c mq.c cache.c triggers.c \
	generate.c entry.c source.c describe.c related.c store.c \
	doc.c licenses.c \
	index.c index-core.c index-about.c index-membership.c \
	index-media.c index-audiences.c

spindle_generate_la_LDFLAGS = -no-undefined -module -avoid-version

spindle_generate_la_LIBADD = ../common/libspindle-common.la \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@ \
	@LIBMQ_LOCAL_LIBS@ @LIBMQ_LIBS@ \
	@LIBRULEBASE_LOCAL_LIBS@ @LIBRULEBASE_LIBS@
