all: bxibase

MKRPMS_DIR = /mnt/mkrpms
TARGET_DOCKER_DIR = /tmp/bxibase.docker

bxibase: bxibase.deps
	echo "Building bxibase rpm"
	${PWD}/../shared/docker/pdp/mkrpms local/centos-bxibase.deps bxibase
	echo "Copying bxibase rpm"
	mkdir -p ${TARGET_DOCKER_DIR}/RPMS/x86_64
	cp Dockerfile.centos-bxibase ${TARGET_DOCKER_DIR}
	cp -a ${MKRPMS_DIR}/RPMS/x86_64/bxibase* ${TARGET_DOCKER_DIR}/RPMS/x86_64/
	echo "Creating the YUM repo"
	createrepo ${TARGET_DOCKER_DIR}/RPMS
	echo "Creating the bxibase docker"
	docker build --tag local/centos-bxibase \
		     --rm=true \
		     --file ${TARGET_DOCKER_DIR}/Dockerfile.centos-bxibase \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		     ${TARGET_DOCKER_DIR}

bxibase.deps: 
	echo "Building docker deps for bxibase"
	docker build --tag local/centos-bxibase.deps \
		     --rm=true \
		     --file Dockerfile.centos-bxibase.deps \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		     ${PWD}
