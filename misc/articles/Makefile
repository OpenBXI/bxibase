all: centos-bxibase-jupyter

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

