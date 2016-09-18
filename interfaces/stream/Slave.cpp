/**
 *-----------------------------------------------------------------------------
 * Title      : Stream interface slave
 * ----------------------------------------------------------------------------
 * File       : Slave.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-09-16
 * Last update: 2016-09-16
 * ----------------------------------------------------------------------------
 * Description:
 * Stream interface slave
 * ----------------------------------------------------------------------------
 * This file is part of the rogue software platform. It is subject to 
 * the license terms in the LICENSE.txt file found in the top-level directory 
 * of this distribution and at: 
 *    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
 * No part of the rogue software platform, including this file, may be 
 * copied, modified, propagated, or distributed except according to the terms 
 * contained in the LICENSE.txt file.
 * ----------------------------------------------------------------------------
**/
#include <unistd.h>
#include <interfaces/stream/Slave.h>
#include <interfaces/stream/Master.h>
#include <interfaces/stream/Buffer.h>
#include <interfaces/stream/Frame.h>
#include <boost/make_shared.hpp>

namespace ris = rogue::interfaces::stream;

//! Class creation
ris::SlavePtr ris::Slave::create () {
   ris::SlavePtr slv = boost::make_shared<ris::Slave>();
   return(slv);
}

//! Creator
ris::Slave::Slave() { 
   allocMeta_  = 0;
   freeMeta_   = 0xFFFFFFFF;
   allocBytes_ = 0;
   allocCount_ = 0;
}

//! Destructor
ris::Slave::~Slave() { }

//! Get allocated memory
uint32_t ris::Slave::getAllocBytes() {
   return(allocBytes_);
}

//! Get allocated count
uint32_t ris::Slave::getAllocCount() {
   return(allocCount_);
}

//! Get allocated memory
void ris::Slave::adjAllocBytes(int32_t adj) {
   allocBytes_ += adj;
}

//! Get allocated count
void ris::Slave::adjAllocCount(int32_t adj) {
   allocCount_ += adj;
}

//! Create a frame
// Frame container should be allocated from shared memory pool
ris::FramePtr ris::Slave::allocFrame ( uint32_t totSize, uint32_t buffSize,
                                       bool compact, bool zeroCopy ) {
   ris::FramePtr  ret;
   ris::BufferPtr buff;
   uint32_t alloc;
   uint32_t bSize;

   ret  = ris::Frame::create(getSlave(),zeroCopy);
   alloc = 0;

   while ( alloc < totSize ) {

      // Don't create larger buffers than we need if compact is set
      if ( (compact == false) || ((totSize-alloc) > buffSize) ) bSize = buffSize;
      else bSize = (totSize-alloc);

      // Create buffer and append to frame
      buff = allocBuffer(bSize);
      alloc += bSize;
      ret->appendBuffer(buff);
   }

   return(ret);
}

//! Create a buffer
// Buffer container and raw data should be allocated from shared memory pool
ris::BufferPtr ris::Slave::allocBuffer ( uint32_t size ) {
   ris::BufferPtr buff;
   uint8_t * data;

   if ( (data = (uint8_t *)malloc(size)) != NULL ) adjAllocBytes(size);
   else size = 0;

   buff = ris::Buffer::create(getSlave(),data,allocMeta_,size);
   adjAllocCount(1);

   // Only use lower 16 bits of meta. 
   // Upper 16 bits may have special meaning to sub-class
   allocMeta_++;
   allocMeta_ &= 0xFFFF;

   return(buff);
}

//! Accept a frame request. Called from master
/*
 * Pass total size required.
 * Pass flag indicating if zero copy buffers are acceptable
 */
ris::FramePtr ris::Slave::acceptReq ( uint32_t size, bool zeroCopyEn, uint32_t timeout) {
   return(allocFrame(size,size,false,false));
}

//! Accept a frame from master
/* 
 * Returns true on success
 */
bool ris::Slave::acceptFrame ( ris::FramePtr frame, uint32_t timeout ) {
   return(false);
}

//! Return a buffer
/*
 * Called when this instance is marked as owner of a Buffer entity
 */
void ris::Slave::retBuffer(uint8_t * data, uint32_t meta, uint32_t rawSize) {
   if ( meta == freeMeta_ ) printf("Buffer return with duplicate meta\n");
   freeMeta_ = meta;

   adjAllocCount(-1);

   if ( data != NULL ) {
      free(data);
      adjAllocBytes(-rawSize);
   }
}

//! Return instance as shared pointer
ris::SlavePtr ris::Slave::getSlave() {
   return(shared_from_this());
}

//! Accept frame
bool ris::SlaveWrap::acceptFrame ( ris::FramePtr frame, uint32_t timeout ) {
   bool ret;
   bool found;

   found = false;
   ret   = false;

   // Not sure if this is (and release) are ok if calling from python to python
   // It appears we need to lock before calling get_override
   PyGILState_STATE pyState = PyGILState_Ensure();

   if (boost::python::override pb = this->get_override("acceptFrame")) {
      found = true;
      try {
         ret = pb(frame,timeout);
      } catch (...) {
         PyErr_Print();
      }
   }
   PyGILState_Release(pyState);

   if ( ! found ) ret = ris::Slave::acceptFrame(frame,timeout);
   return(ret);
}

//! Default accept frame call
bool ris::SlaveWrap::defAcceptFrame ( ris::FramePtr frame, uint32_t timeout ) {
   return(ris::Slave::acceptFrame(frame,timeout));
}

