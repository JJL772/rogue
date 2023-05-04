FROM tidair/rogue-base:v3.0.0

# Install Rogue
ARG branch
WORKDIR /usr/local/src
RUN git clone https://github.com/slaclab/rogue.git -b $branch
WORKDIR rogue
RUN mkdir build
WORKDIR build
RUN cmake .. -DROGUE_INSTALL=system -DDO_EPICS=1
RUN make -j4 install
RUN ldconfig
WORKDIR /root
