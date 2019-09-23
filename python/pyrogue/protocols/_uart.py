#!/usr/bin/env python
#-----------------------------------------------------------------------------
# Title      : PyRogue protocols / UART register protocol
#-----------------------------------------------------------------------------
# File       : pyrogue/protocols/_uart.py
# Created    : 2017-01-15
#-----------------------------------------------------------------------------
# Description:
# Module containing protocol modules
#-----------------------------------------------------------------------------
# This file is part of the rogue software platform. It is subject to
# the license terms in the LICENSE.txt file found in the top-level directory
# of this distribution and at:
#    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# No part of the rogue software platform, including this file, may be
# copied, modified, propagated, or distributed except according to the terms
# contained in the LICENSE.txt file.
#-----------------------------------------------------------------------------

import rogue.interfaces.memory
import pyrogue
import serial
import queue
import threading

class UartMemory(rogue.interfaces.memory.Slave):
    def __init__(self, device, baud, timeout=1, **kwargs):
        super().__init__(4,4096) # Set min and max size to 4 bytes

        self._log = pyrogue.logInit(cls=self, name=f'{device}')
        self.serialPort = serial.Serial(device, baud, timeout=timeout, **kwargs)

        self._workerQueue = queue.Queue()
        self._workerThread = threading.Thread(target=self._worker)
        self._workerThread.start()

    def close(self):
        self._workerQueue.put(None)
        self._workerQueue.join()
        self.serialPort.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def readline(self):
        l = []
        while True:
            ch = self.serialPort.read().decode('ASCII')
            l.append(ch)
            if ch == '\n' or ch == '\r':
                break
        return ''.join(l)

    def _doTransaction(self, transaction):
        self._workerQueue.put(transaction)

    def _worker(self):
        while True:
            transaction = self._workerQueue.get()

            if transaction is None:
                break;

            with transaction.lock():
                address = transaction.address()

                # Write path
                if transaction.type() == rogue.interfaces.memory.Write:
                    dataBa = bytearray(transaction.size())
                    transaction.getData(dataBa, 0)
                    dataWords = [int.from_bytes(dataBa[i:i+4], 'little', signed=False) for i in range(0, len(dataBa), 4)]

                    # Need to issue a UART command for each 32 bit word
                    for i, (addr, data) in enumerate(zip(range(address, address+len(dataBa), 4), dataWords)):
                        sendString = f'w {addr:08x} {data:08x} \n'.encode('ASCII')
                        # self._log.debug(f'Sending write transaction part {i}: {repr(sendString)}')
                        self.serialPort.write(sendString)
                        response = self.readline() #self.serialPort.readline().decode('ASCII')
                        
                        # If response is empty, a timeout occured
                        if len(response) == 0:
                            transaction.error(f'Empty transaction response (likely timeout) for transaction part {i}: {repr(sendString)}')
                            return

                        # parse the response string
                        parts = response.split()
                        # Check for correct response
                        if (len(parts) != 4 or
                            parts[0].lower() != 'w' or
                            int(parts[1], 16) != addr):
                            transaction.error(f'Malformed response for part {i}: {repr(response)} to transaction: {repr(sendString)}')
                            return
                        # else:
                            # self._log.debug(f'Transaction part {i}: {repr(sendString)} completed successfully')
                            
                    transaction.done()                        

                elif (transaction.type() == rogue.interfaces.memory.Read or
                     transaction.type() == rogue.interfaces.memory.Verify):
                    size = transaction.size()

                    for i, addr in enumerate(range(address, address+size, 4)):
                        sendString = f'r {addr:08x} \n'.encode('ASCII')
                        # self._log.debug(f'Sending read transaction part {i}: {repr(sendString)}')
                        self.serialPort.write(sendString)
                        response = self.readline() #self.serialPort.readline().decode('ASCII')

                        # If response is empty, a timeout occured
                        if len(response) == 0:
                            transaction.error(f'Empty transaction response (likely timeout) for transaction part {i}: {repr(sendString)}')
                            return
                        
                        # parse the response string
                        parts = response.split()

                        if (len(parts) != 4 or
                            parts[0].lower() != 'r' or
                            int(parts[1], 16) != addr):
                            transaction.error(f'Malformed response part {i}: {repr(response)} to transaction: {repr(sendString)}')
                        else:
                            dataInt = int(parts[2], 16)
                            rdData = bytearray(dataInt.to_bytes(4, 'little', signed=False))
                            transaction.setData(rdData, i*4)
                            # self._log.debug(f'Transaction part {i}: {repr(sendString)} with response data: {dataInt:#08x} completed successfully')
                        
                    transaction.done()
                else:
                    # Posted writes not supported (for now)
                    transaction.error(f'Unsupported transaction type: {transaction.type()}')

