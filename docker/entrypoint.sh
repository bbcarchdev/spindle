#!/bin/bash
# Init script for postgres that enables some modules

set -e

if [ "$1" = 'spindle' ]; then
	echo "Sleeping for 5 seconds to make sure database is initialised.."
	sleep 5
	/usr/bin/twine -S || exit $?
	exec /usr/sbin/apache2 -DFOREGROUND
fi

exec "$@"

