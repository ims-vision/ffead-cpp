FROM buildpack-deps:xenial

ENV IROOT=/installs
ENV FFEAD_CPP_PATH=${IROOT}/ffead-cpp-2.0
ENV PATH=${FFEAD_CPP_PATH}:${PATH}
ENV FFEAD_TEST_TYPE=mongo
ENV FFEAD_TEST_DB_NAME=mongo
ENV FFEAD_ENABLE_SDORM_SQL="no"
ENV FFEAD_ENABLE_SDORM_MONGO="yes"
ENV FFEAD_ENABLE_APACHEMOD="no"
ENV FFEAD_ENABLE_NGINXMOD="no"
ENV FFEAD_ENABLE_REDIS="no"
ENV FFEAD_ENABLE_MEMCACHED="yes"
ENV FFEAD_ENABLE_DEBUG="no"

RUN mkdir /installs

WORKDIR /

COPY te-benchmark/ te-benchmark/
COPY *.sh ./
RUN chmod 755 *.sh

RUN ./ffead-cpp-dependencies.sh

WORKDIR /

RUN ./ffead-cpp-memcached.sh

WORKDIR /

RUN ./ffead-cpp-framework.sh

RUN cp -f ${IROOT}/ffead-cpp-2.0/web/te-benchmark/config/cachememcached.xml ${IROOT}/ffead-cpp-2.0/web/te-benchmark/config/cache.xml

WORKDIR ${IROOT}/ffead-cpp-2.0

CMD ./server.sh
