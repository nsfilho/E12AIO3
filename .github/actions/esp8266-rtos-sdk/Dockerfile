FROM debian:buster
LABEL AUTHOR_NAME "Nelio Souza Santos Filho"
LABEL AUTHOR_EMAIL "nsfilho@icloud.com"

# Variaveis de ambiente
ENV SDK_URL=https://github.com/espressif/ESP8266_RTOS_SDK.git
ENV XTENSA_URL=https://dl.espressif.com/dl/xtensa-lx106-elf-linux64-1.22.0-100-ge567ec7-5.2.0.tar.gz
ENV IDF_PATH=/esp/sdk
ENV PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:/esp/sdk/components/esptool_py/esptool:/esp/sdk/components/partition_table:/esp/sdk/tools:/esp/xtensa-lx106-elf/bin

ADD entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT [ "/entrypoint.sh" ]

# Instalando pacotes padrões
RUN apt-get update && apt-get install -y \
    cmake ninja-build git \
    curl wget python-pip \
    libncurses-dev build-essential flex bison gperf \
    gcc make python python-serial

# Baixa o release do sdk
RUN mkdir /esp /esp/sdk && \
    git clone -b release/v3.3 --recursive ${SDK_URL} /esp/sdk && \
    cd /esp/sdk && \
    pip install -r requirements.txt

# Baixa os toolchains
RUN curl -o /esp/xtensa.tar.gz ${XTENSA_URL} && \
    cd /esp && \
    tar xzvf xtensa.tar.gz && \
    rm -f /esp/xtensa.tar.gz


