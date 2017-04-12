all: bxibase

MKRPMS_DIR := $(shell cat ~/.config/mkrpms.conf|grep RPMS_DIR|cut -d '=' -f 2)
TARGET_DOCKER_DIR = /tmp/bxibase.docker

bxibase: 
	echo "Building bxibase rpm"
	$(CURDIR)/../shared/docker/mkrpms/mkrpms openbxi/centos-bxibase-devel bxibase
	echo "Copying bxibase rpm"
	mkdir -p ${TARGET_DOCKER_DIR}/RPMS/x86_64
	cp Dockerfile.centos-bxibase* ${TARGET_DOCKER_DIR}
	cp -a ${MKRPMS_DIR}/RPMS/x86_64/bxibase* ${TARGET_DOCKER_DIR}/RPMS/x86_64/
	echo "Creating the YUM repo"
	createrepo ${TARGET_DOCKER_DIR}/RPMS
	echo "Creating the bxibase docker"

	docker build --tag openbxi/centos-bxibase \
		     --rm=true \
		     --file ${TARGET_DOCKER_DIR}/Dockerfile.centos-bxibase \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		     ${TARGET_DOCKER_DIR}

	echo "Building docker for bxibase-devel"
	docker build --tag openbxi/centos-bxibase-devel \
		     --rm=true \
		     --file ${TARGET_DOCKER_DIR}/Dockerfile.centos-bxibase-devel \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		     ${TARGET_DOCKER_DIR}
