/**
 *-----------------------------------------------------------------------------
 * Title      : Raw Memory Mapped Access
 * ----------------------------------------------------------------------------
 * File       : MemMap.cpp
 * Created    : 2019-11-18
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
#include <rogue/hardware/MemMap.h>
#include <rogue/interfaces/memory/Constants.h>
#include <rogue/interfaces/memory/Transaction.h>
#include <rogue/interfaces/memory/TransactionLock.h>
#include <rogue/GeneralError.h>
#include <rogue/GilRelease.h>
#include <memory>
#include <cstring>
#include <thread>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

namespace rh  = rogue::hardware;
namespace rim = rogue::interfaces::memory;

#ifndef NO_PYTHON
#include <boost/python.hpp>
namespace bp  = boost::python;
#endif

//! Class creation
rh::MemMapPtr rh::MemMap::create (uint64_t base, uint32_t size) {
   rh::MemMapPtr r = std::make_shared<rh::MemMap>(base,size);
   return(r);
}

//! Creator
rh::MemMap::MemMap(uint64_t base, uint32_t size) : rim::Slave(4,0xFFFFFFFF) {
   log_ = rogue::Logging::create("MemMap");

   size_ = size;

   fd_ = ::open(MAP_DEVICE, O_RDWR);

   if ( fd_ < 0 ) 
      throw(rogue::GeneralError::create("MemMap::MemMap", "Failed to open device file: %s",MAP_DEVICE));

   if ( (map_ = (uint8_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, base)) == (void *) -1)
      throw(rogue::GeneralError::create("MemMap::MemMap", "Failed to map memory to user space."));

   log_->debug("Created map to 0x%x with size 0x%x", base, size);
}

//! Destructor
rh::MemMap::~MemMap() {
   munmap((void *)map_,size_);
   ::close(fd_);
}

//! Post a transaction
void rh::MemMap::doTransaction(rim::TransactionPtr tran) {
   uint32_t * tPtr;
   uint32_t * mPtr;
   uint32_t   count = 0;

   rogue::GilRelease noGil;
   rim::TransactionLockPtr lock = tran->lock();

   if ( (tran->size() % 4) != 0 ) {
      tran->error("Invalid transaction size %i, must be an integer number of 4 bytes",tran->size());
      return;
   }

   // Check that the address is legal
   if ( (tran->address() + tran->size()) > size_ ) {
      tran->error("Request transaction to address 0x%x with size %i is out of bounds",tran->address(),tran->size());
      return;
   }

   tPtr = (uint32_t *)tran->begin();
   mPtr = (uint32_t *)(map_ + tran->address());

   while ( count != tran->size() ) {

      // Write or post
      if (tran->type() == rim::Write || tran->type() == rim::Post) *mPtr = *tPtr;

      // Read or verify
      else *tPtr = *mPtr;

      ++mPtr;
      ++tPtr;
      count += 4;
   }

   log_->debug("Transaction id=0x%08x, addr 0x%08x. Size=%i, type=%i",tran->id(),tran->address(),tran->size(),tran->type());
   tran->done();
}

void rh::MemMap::setup_python () {
#ifndef NO_PYTHON

   bp::class_<rh::MemMap, rh::MemMapPtr, bp::bases<rim::Slave>, boost::noncopyable >("MemMap",bp::init<uint64_t, uint32_t>());

   bp::implicitly_convertible<rh::MemMapPtr, rim::SlavePtr>();
#endif
}
