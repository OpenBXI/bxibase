all: epel pdp jupyter jupyter-c bxibase

epel:
	docker build -t local/centos-epel -f Dockerfile.centos-epel \
				                                    --build-arg HTTP_PROXY=$(http_proxy)\
								    --build-arg HTTPS_PROXY=${https_proxy}\
								    --build-arg FTP_PROXY=${ftp_proxy}\
								    --build-arg NO_PROXY=${no_proxy}\
								    .

pdp: epel
	docker build -t local/centos-pdp -f Dockerfile.centos-pdp \
								    --build-arg HTTP_PROXY=$(http_proxy)\
								    --build-arg HTTPS_PROXY=${https_proxy}\
								    --build-arg FTP_PROXY=${ftp_proxy}\
								    --build-arg NO_PROXY=${no_proxy}\
								    .

jupyter: epel
	docker build -t local/jupyter -f Dockerfile.jupyter \
								    --build-arg HTTP_PROXY=$(http_proxy)\
								    --build-arg HTTPS_PROXY=${https_proxy}\
								    --build-arg FTP_PROXY=${ftp_proxy}\
								    --build-arg NO_PROXY=${no_proxy}\
								    .

jupyter-c: jupyter
	docker build -t local/jupyter-c -f Dockerfile.jupyter-c \
								    --build-arg HTTP_PROXY=$(http_proxy)\
								    --build-arg HTTPS_PROXY=${https_proxy}\
								    --build-arg FTP_PROXY=${ftp_proxy}\
								    --build-arg NO_PROXY=${no_proxy}\
								    .

bxibase: 
	docker build -t local/bxibase -f Dockerfile.bxibase \
								    --build-arg HTTP_PROXY=$(http_proxy)\
								    --build-arg HTTPS_PROXY=${https_proxy}\
								    --build-arg FTP_PROXY=${ftp_proxy}\
								    --build-arg NO_PROXY=${no_proxy}\
								    .									