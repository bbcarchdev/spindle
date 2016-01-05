#!/bin/bash
# Init script for postgres that enables some modules

set -e

if [ "$1" = 'spindle' ]; then
	/usr/bin/twine -S || exit $?
	exec /usr/sbin/apache2 -DFOREGROUND
fi

exec "$@"

