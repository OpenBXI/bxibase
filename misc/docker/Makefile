all: bxibase-docker

bxibase-docker: bxibase
	docker rm -f bxibase || /bin/true
	docker run  --name bxibase --publish 8080:80 --detach --volume ${PWD}/../articles:/srv local/bxibase

bxibase: jupyter-c
	echo "Building jupyter-c"
	docker build --tag local/bxibase --rm=true --file Dockerfile.centos-bxibase ${PWD}

jupyter-c: jupyter
	echo "Building jupyter"
	docker build --tag local/jupyter-c --rm=true --file Dockerfile.centos-jupyter-c ${PWD}

jupyter: centos-pdp
	echo "Building centos-pdp"
	docker build --tag local/jupyter --rm=true --file Dockerfile.centos-jupyter ${PWD}

centos-pdp:
	echo "Building centos-pdp"
	docker build --tag local/pdp --rm=true --file Dockerfile.centos-epel ${PWD}
