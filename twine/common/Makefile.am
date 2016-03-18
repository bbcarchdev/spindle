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

AM_CPPFLAGS = @AM_CPPFLAGS@ @LIBTWINE_CPPFLAGS@

noinst_LTLIBRARIES = libspindle-common.la

libspindle_common_la_SOURCES = p_spindle.h spindle-common.h \
	context.c db-common.c db-schema.c db-correlate.c rulebase.c \
	rulebase-class.c rulebase-pred.c rulebase-cachepred.c \
	rulebase-coref.c strset.c correlate.c graphcache.c

libspindle_common_la_LIBADD = @LIBTWINE_LOCAL_LIBS@ @LIBTWINE_LIBS@ \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@ \
	@LIBSQL_LOCAL_LIBS@ @LIBSQL_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@
