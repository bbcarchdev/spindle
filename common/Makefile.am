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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	@LIBRULEBASE_CPPFLAGS@ \
	@LIBTWINE_CPPFLAGS@

noinst_LTLIBRARIES = libspindle-common.la

libspindle_common_la_SOURCES = p_spindle.h spindle-common.h \
	context.c db-common.c db-schema.c db-correlate.c \
	correlate.c graphcache.c

libspindle_common_la_LIBADD = \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@ \
	@LIBRULEBASE_LOCAL_LIBS@ @LIBRULEBASE_LIBS@ \
	@LIBSQL_LOCAL_LIBS@ @LIBSQL_LIBS@ \
	@LIBTWINE_LOCAL_LIBS@ @LIBTWINE_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@
