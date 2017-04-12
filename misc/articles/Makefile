all: centos-bxibase-jupyter

MKRPMS_DIR := $(shell cat ~/.config/mkrpms.conf|grep RPMS_DIR|cut -d '=' -f 2)
TARGET_DOCKER_DIR = /tmp/bxibase-jupyter.docker

centos-bxibase-jupyter: 
	echo "Building bxibase rpms"
	$(CURDIR)/../shared/docker/mkrpms/mkrpms openbxi/centos-bxibase-devel bxibase
	echo "Copying bxibase rpm"
	mkdir -p ${TARGET_DOCKER_DIR}/RPMS/x86_64
	cp Dockerfile.centos-bxibase* ${TARGET_DOCKER_DIR}
	cp -a ${MKRPMS_DIR}/RPMS/x86_64/bxibase* ${TARGET_DOCKER_DIR}/RPMS/x86_64/
	echo "Creating the YUM repo"
	createrepo ${TARGET_DOCKER_DIR}/RPMS
	echo "Building centos-bxibase-jupyter"

	docker build --tag openbxi/centos-bxibase-jupyter \
		     --rm=true \
		     --file ${TARGET_DOCKER_DIR}/Dockerfile.centos-bxibase-jupyter \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		    ${TARGET_DOCKER_DIR}

# centos-bxibase-jupyter-c: 
# 	echo "Building centos-bxibase-jupyter-c"
# 	docker build --tag openbxi/centos-bxibase-jupyter-c \
# 		     --rm=true \
# 		     --file Dockerfile.centos-bxibase-jupyter-c \
# 		     --build-arg HTTP_PROXY=$(http_proxy)\
# 		     --build-arg HTTPS_PROXY=${https_proxy}\
# 		     --build-arg FTP_PROXY=${ftp_proxy}\
# 		     --build-arg NO_PROXY=${no_proxy}\
# 		    ${PWD}

