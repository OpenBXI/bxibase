all: centos-bxibase-jupyter centos-bxibase-jupyter-c alpine-bxibase-jupyter

MKRPMS_DIR := $(shell cat ~/.config/mkrpms.conf|grep RPMS_DIR|cut -d '=' -f 2)
TARGET_DOCKER_DIR = /tmp/bxibase-jupyter.docker

# This target is for Python2 only based notebooks
centos-bxibase-jupyter: 
	echo "Building centos-bxibase-jupyter"
	docker build --tag openbxi/centos-bxibase-jupyter \
		     --rm=true \
		     --file Dockerfile.centos-bxibase-jupyter \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		    ${PWD}



# This target is for C only based notebooks (C jupyter kernel requires Python3)
centos-bxibase-jupyter-c: 
	echo "Building bxibase rpms"
	$(CURDIR)/../shared/docker/mkrpms/mkrpms openbxi/centos-bxibase-devel bxibase
	echo "Copying bxibase rpm"
	mkdir -p ${TARGET_DOCKER_DIR}/RPMS/x86_64
	cp Dockerfile.centos-bxibase* ${TARGET_DOCKER_DIR}
	cp -a ${MKRPMS_DIR}/RPMS/x86_64/bxibase* ${TARGET_DOCKER_DIR}/RPMS/x86_64/
	echo "Creating the YUM repo"
	createrepo ${TARGET_DOCKER_DIR}/RPMS
	echo "Building centos-bxibase-jupyter"

	docker build --tag openbxi/centos-bxibase-jupyter-c \
		     --rm=true \
		     --file ${TARGET_DOCKER_DIR}/Dockerfile.centos-bxibase-jupyter-c \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		    ${TARGET_DOCKER_DIR}

alpine-bxibase-jupyter: 
	echo "Building alpine-bxibase-jupyter"

	docker build --tag openbxi/alpine-bxibase-jupyter \
		     --rm=true \
		     --file Dockerfile.alpine-bxibase-jupyter \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		    .
