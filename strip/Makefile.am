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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	@LIBRULEBASE_CPPFLAGS@ \
	@LIBTWINE_CPPFLAGS@ \
	-I$(srcdir)/../common

twinemodule_LTLIBRARIES = spindle-strip.la

# The spindle-strip module is a processor which removes triples that the
# rulebase does not indicate should be retained.

spindle_strip_la_SOURCES = p_spindle-strip.h \
	strip-module.c processor.c

spindle_strip_la_LDFLAGS = -no-undefined -module -avoid-version

spindle_strip_la_LIBADD = ../common/libspindle-common.la \
	@LIBRULEBASE_LOCAL_LIBS@ @LIBRULEBASE_LIBS@
