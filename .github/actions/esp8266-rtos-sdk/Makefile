all:
	docker build --rm=true --force-rm=true -t nsfilho/esp8266-rtos-sdk:3.3 ./

run:
	cd ../../../ && docker run -it -v `pwd -P`:/app nsfilho/esp8266-rtos-sdk:3.3 /bin/bash

remove:
	docker stop nsfilho/esp8266-rtos-sdk
	docker rm nsfilho/esp8266-rtos-sdk

push:
	docker push nsfilho/esp8266-rtos-sdk:3.3
