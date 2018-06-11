##  Spindle: The RES Linked Open Data Aggregator
##
##  Author: Mo McRoberts
##
##  Copyright (c) 2014-2015 BBC
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

AC_INIT([spindle],m4_esyscmd([/bin/sh m4/get-version.sh]),[RESFeedback@bbc.co.uk],,[https://github.com/bbcarchdev/spindle])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC([clang c99 gcc cc])
BT_PROG_CC_WARN
BT_PROG_CC_DEBUG
AC_PROG_CC_C99

LT_INIT

BT_ENABLE_POSIX_FULL
AC_SYS_LARGEFILE

dnl These libraries are used by all built components
BT_REQUIRE_LIBUUID
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBURI
BT_REQUIRE_LIBSQL
BT_REQUIRE_LIBAWSCLIENT
BT_REQUIRE_LIBSPARQLCLIENT

dnl These libraries are linked conditionally
BT_REQUIRE_LIBTWINE([true])
BT_REQUIRE_LIBQUILT([true])
BT_REQUIRE_LIBMQ([true])

BT_DEFINE_PATH([TWINEMODULEDIR], [twinemoduledir], [Twine module path])
BT_DEFINE_PATH([QUILTMODULEDIR], [quiltmoduledir], [Quilt module path])

use_docbook_html5=yes
BT_BUILD_DOCS

XML2HTMLFLAGS="--param \"html.linksfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-links.xml'\" \
  --param \"html.navfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-nav.xml'\" \
  --param \"html.ie78css\" \"'//bbcarchdev.github.io/painting-by-numbers/ie78.css'\""
AC_SUBST([XML2HTMLFLAGS])

LINKS='${top_srcdir}/docbook-html5/res-links.xml'
AC_SUBST([LINKS])

NAV='${top_srcdir}/docbook-html5/res-nav.xml'
AC_SUBST([NAV])

spindlehtdocdir='${docdir}'
AC_SUBST([spindlehtdocdir])

BT_DEFINE_PREFIX

# Build options

AC_DEFINE_UNQUOTED([SPINDLE_ENABLE_ABOUT_SELF], [0], [Whether to enable 'items are about themselves' in the index])

AC_CONFIG_FILES([Makefile
m4/Makefile
twine/Makefile
twine/common/Makefile
twine/strip/Makefile
twine/correlate/Makefile
twine/generate/Makefile
quilt/Makefile
migrate/Makefile
docbook-html5/Makefile
docs/Makefile
])

AC_OUTPUT
