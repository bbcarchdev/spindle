#!/bin/bash
docker-compose -f docker/integration.yml.default -f docker/development.yml.default -p spindle stop
yes|docker-compose -f docker/integration.yml.default -f docker/development.yml.default -p spindle rm
docker-compose -f docker/integration.yml.default -f docker/development.yml.default -p spindle run cucumber
docker-compose -f docker/integration.yml.default -f docker/development.yml.default -p spindle run cucumber

