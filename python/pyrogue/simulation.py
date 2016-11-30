#!/usr/bin/env python
#-----------------------------------------------------------------------------
# Title      : PyRogue simulation support
#-----------------------------------------------------------------------------
# File       : pyrogue/simulation.py
# Author     : Ryan Herbst, rherbst@slac.stanford.edu
# Created    : 2016-09-29
# Last update: 2016-09-29
#-----------------------------------------------------------------------------
# Description:
# Module containing simulation support classes and routines
#-----------------------------------------------------------------------------
# This file is part of the rogue software platform. It is subject to 
# the license terms in the LICENSE.txt file found in the top-level directory 
# of this distribution and at: 
#    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
# No part of the rogue software platform, including this file, may be 
# copied, modified, propagated, or distributed except according to the terms 
# contained in the LICENSE.txt file.
#-----------------------------------------------------------------------------
import threading
import pyrogue
import rogue.interfaces.stream
import time
import zmq

class StreamSim(rogue.interfaces.stream.Master, 
                rogue.interfaces.stream.Slave, 
                threading.Thread):

    def __init__(self,host,dest,ssi=False):
        rogue.interfaces.stream.Master.__init__(self)
        rogue.interfaces.stream.Slave.__init__(self)
        threading.Thread.__init__(self)
        
        self._ctx = zmq.Context()
        self._ibSock = self._ctx.socket(zmq.REP)
        self._obSock = self._ctx.socket(zmq.REQ)
        self._ibSock.connect("tcp://%s:%i" % (host,dest+5000))
        self._obSock.connect("tcp://%s:%i" % (host,dest+6000))

        self._ssi     = ssi
        self._enable  = True
        self.rxCount  = 0
        self.txCount  = 0

        self.start()

    def stop(self):
        self._enable = False

    def _acceptFrame(self,frame):
        """ Forward frame to simulation """

        flags = frame.getFlags()
        fuser = flags & 0xFF
        luser = (flags >> 8) & 0xFF

        if ( self._ssi ):
            fuser = fuser | 0x2 # SOF
            if ( frame.getError() ): luser = luser | 0x1 # EOFE

        ba = bytearray(1)
        bb = bytearray(1)
        bc = bytearray(frame.getPayload())

        ba[0] = fuser
        bb[0] = luser
        frame.read(bc,0)

        self._obSock.send(ba,zmq.SNDMORE)
        self._obSock.send(bb,zmq.SNDMORE)
        self._obSock.send(bc)
        self.txCount += 1

        # Wait for ack
        self._obSock.recv_multipart()

    def run(self):
        """Receive frame from simulation"""

        while(self._enable):
            r = self._ibSock.recv_multipart()

            fuser = bytearray(r[0])[0]
            luser = bytearray(r[1])[0]
            data  = bytearray(r[2])

            frame = self._reqFrame(len(data),True,0)

            if ( self._ssi and (luser & 0x1)): frame.setError(1)

            frame.write(data,0)
            self._sendFrame(frame)
            self.rxCount += 1

            # Send ack
            ba = bytearray(1)
            ba[0] = 0xFF
            self._ibSock.send(ba)
