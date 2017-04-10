all: hlpic.run

hlpic.run: 
	echo "Running hlpic -- You cann access it through http://localhost:8080"
	docker rm -f hlpic || /bin/true
	docker run  --name hlpic --publish 8080:8080 --detach --volume ${PWD}:/srv openbxi/centos-bxibase-jupyter


