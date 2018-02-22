## Spindle: The RES Linked Open Data Aggregation Engine
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

AM_CPPFLAGS = @AM_CPPFLAGS@ @LIBTWINE_CPPFLAGS@ -I$(srcdir)/../common

noinst_LTLIBRARIES = librulebase.la

librulebase_la_SOURCES = rulebase.h rulebase.c \
	rulebase-cachepred.c rulebase-class.c rulebase-coref.c \
	rulebase-pred.c strset.c

librulebase_la_LIBADD = @LIBTWINE_LOCAL_LIBS@ @LIBTWINE_LIBS@ \
	@LIBAWSCLIENT_LOCAL_LIBS@ @LIBAWSCLIENT_LIBS@ \
	@LIBSQL_LOCAL_LIBS@ @LIBSQL_LIBS@ \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@
