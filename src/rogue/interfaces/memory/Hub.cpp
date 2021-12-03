/**
 *-----------------------------------------------------------------------------
 * Title      : Hub
 * ----------------------------------------------------------------------------
 * File       : Hub.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-09-20
 * Last update: 2016-09-20
 * ----------------------------------------------------------------------------
 * Description:
 * A memory interface hub. Accepts requests from multiple masters and forwards
 * them to a downstream slave. Address is updated along the way. Includes support
 * for modification callbacks.
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
#include <rogue/interfaces/memory/Hub.h>
#include <rogue/interfaces/memory/Transaction.h>
#include <rogue/GilRelease.h>
#include <rogue/ScopedGil.h>
#include <memory>

namespace rim = rogue::interfaces::memory;

namespace rogue {
   namespace interfaces {
      namespace memory {
         using TransactionQueue     = std::queue<TransactionPtr>;
         using TransactionStatusMap = std::map<uint32_t,bool>;
      }
   }
}

#ifndef NO_PYTHON
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/python.hpp>
namespace bp  = boost::python;
#endif

//! Create a block, class creator
rim::HubPtr rim::Hub::create (uint64_t offset, uint32_t min, uint32_t max) {
   rim::HubPtr b = std::make_shared<rim::Hub>(offset,min,max);
   return(b);
}

//! Create an block
rim::Hub::Hub(uint64_t offset, uint32_t min, uint32_t max) : Master (), Slave(min,max) {
   offset_ = offset;
   root_   = (min != 0 && max != 0);
}

//! Destroy a block
rim::Hub::~Hub() { }

//! Get offset
uint64_t rim::Hub::getOffset() {
   return offset_;
}

//! Get address
uint64_t rim::Hub::getAddress() {
   return(reqAddress() | offset_);
}

//! Return id to requesting master
uint32_t rim::Hub::doSlaveId() {
   if ( root_ ) return(rim::Slave::doSlaveId());
   else return(reqSlaveId());
}

//! Return name to requesting master
std::string rim::Hub::doSlaveName() {
   if ( root_ ) return(rim::Slave::doSlaveName());
   else return(reqSlaveName());
}

//! Return min access size to requesting master
uint32_t rim::Hub::doMinAccess() {
   if ( root_ ) return(rim::Slave::doMinAccess());
   else return(reqMinAccess());
}

//! Return max access size to requesting master
uint32_t rim::Hub::doMaxAccess() {
   if ( root_ ) return(rim::Slave::doMaxAccess());
   else return(reqMaxAccess());
}

//! Return address
uint64_t rim::Hub::doAddress() {
   if ( root_ ) return(0);
   else return(reqAddress() | offset_);
}

//! Create new transactions. We call this method from rim::Hub::doTransaction or a Hub sub-class.
rim::TransactionQueue rim::Hub::processTransaction(rim::TransactionPtr tran, uint32_t limit=4096, uint32_t offset=0x1000) {

   // Queue to store the new transactions
   TransactionQueue transQueue;

   // Map to keep track of the status of each sub-transaction
   TransactionStatusMap transStatMap;

   // Compute number of protocol-compliant transactions required
   auto numberOfTransactions = std::ceil(tran->size() / limit);

   // Create new transactions
   for (unsigned int i=0; i<numberOfTransactions; ++i)
   {
      // Create the new sub-transaction
      auto subtran = rim::Transaction::create(tran->timeout_);

      subtran->iter_    = (uint8_t *) tran->address() + i * offset;
      subtran->size_    = limit;
      subtran->address_ = this->getAddress() + i * offset;
      subtran->type_    = tran->type();

      // Add sub-transaction to the queue
      transQueue.push(subtran);

      // Schedule the new sub-transaction
      auto id = this->intTransaction(subtran);
      
      // Wait for sub-transaction to complete
      //this->waitTransaction(id);

      // Check transaction result
      //if (this->getError() != "")
      //{
      //   subtran->error(this->getError().c_str());
      //   transStatMap[id] = false;
      //}
      //else
      //{
      //   subtran->done();
      //   transStatMap[id] = true;
      //}
   }
   return transQueue;
}

//! Post a transaction. Master will call this method with the access attributes.
void rim::Hub::doTransaction(rim::TransactionPtr tran) {

   // Adjust address
   tran->address_ |= offset_;
   
   // Pre-process the transaction
   auto transQueue = this->processTransaction(tran);

   // Forward transaction
   while (!transQueue.empty())
   {
      getSlave()->doTransaction(transQueue.front());
      transQueue.pop();
   }
}

void rim::Hub::setup_python() {

#ifndef NO_PYTHON
   bp::class_<rim::HubWrap, rim::HubWrapPtr, bp::bases<rim::Master,rim::Slave>, boost::noncopyable>("Hub",bp::init<uint64_t,uint32_t,uint32_t>())
       .def("_blkMinAccess",  &rim::Hub::doMinAccess)
       .def("_blkMaxAccess",  &rim::Hub::doMaxAccess)
       .def("_getAddress",    &rim::Hub::getAddress)
       .def("_getOffset",     &rim::Hub::getOffset)
       .def("_doTransaction", &rim::Hub::doTransaction, &rim::HubWrap::defDoTransaction)
   ;

   bp::implicitly_convertible<rim::HubPtr, rim::MasterPtr>();
#endif
}

#ifndef NO_PYTHON

//! Constructor
rim::HubWrap::HubWrap(uint64_t offset, uint32_t min, uint32_t max) : rim::Hub(offset,min,max) {}

//! Post a transaction. Master will call this method with the access attributes.
void rim::HubWrap::doTransaction(rim::TransactionPtr transaction) {
   {
      rogue::ScopedGil gil;

      if (boost::python::override pb = this->get_override("_doTransaction")) {
         try {
            pb(transaction);
            return;
         } catch (...) {
            PyErr_Print();
         }
      }
   }
   rim::Hub::doTransaction(transaction);
}

//! Post a transaction. Master will call this method with the access attributes.
void rim::HubWrap::defDoTransaction(rim::TransactionPtr transaction) {
   rim::Hub::doTransaction(transaction);
}

#endif

