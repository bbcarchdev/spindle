#!/bin/bash -ex

# Set some variables
DOCKER_REGISTRY="vm-10-100-0-25.ch.bbcarchdev.net"
PROJECT_NAME="spindle"
PROJECT_NAME_TWINE="twine-${PROJECT_NAME}"
PROJECT_NAME_QUILT="quilt-${PROJECT_NAME}"
INTEGRATION="docker/integration.yml"

# Build the project (in dev mode for now)
docker build -t ${PROJECT_NAME_TWINE} -f docker/Dockerfile-twine-build .
# Build the project (in dev mode for now)
docker build -t ${PROJECT_NAME_QUILT} -f docker/Dockerfile-quilt-build .

# If successfully built, tag and push to registry
if [ ! "${JENKINS_HOME}" = '' ]
then
        docker tag -f ${PROJECT_NAME_TWINE} ${DOCKER_REGISTRY}/${PROJECT_NAME_TWINE}
        docker push ${DOCKER_REGISTRY}/${PROJECT_NAME_TWINE}

        docker tag -f ${PROJECT_NAME_QUILT} ${DOCKER_REGISTRY}/${PROJECT_NAME_QUILT}
        docker push ${DOCKER_REGISTRY}/${PROJECT_NAME_QUILT}
fi

if [ -f "${INTEGRATION}.default" ]
then
	# Copy the YML script and adjust the paths
	cp ${INTEGRATION}.default ${INTEGRATION}

	if [ ! "${JENKINS_HOME}" = '' ]
	then
		# Change "in-container" mount path to host mount path
                sed -i -e "s|- \./|- ${HOST_DATADIR}jobs/${JOB_NAME}/workspace/docker/|" "${INTEGRATION}"
	fi

	# Tear down integration from previous run if it was still running
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} stop
	docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} rm -f

        # Start project integration
        docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} run cucumber

        # Tear down integration
        docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} stop
        docker-compose -p ${PROJECT_NAME}-test -f ${INTEGRATION} rm -f
fi
