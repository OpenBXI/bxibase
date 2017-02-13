all: jupyter-c

jupyter-c: jupyter
	echo "Building jupyter-c"
	docker build --tag local/centos-jupyter-c \
		     --rm=true \
		     --file Dockerfile.centos-jupyter-c \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		     ${PWD}

jupyter: 
	echo "Building jupyter"
	docker build --tag local/centos-jupyter \
		     --rm=true \
		     --file Dockerfile.centos-jupyter \
		     --build-arg HTTP_PROXY=$(http_proxy)\
		     --build-arg HTTPS_PROXY=${https_proxy}\
		     --build-arg FTP_PROXY=${ftp_proxy}\
		     --build-arg NO_PROXY=${no_proxy}\
		    ${PWD}

