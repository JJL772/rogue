/**
 *-----------------------------------------------------------------------------
 * Title      : EXO TEM Base Class
 * ----------------------------------------------------------------------------
 * File       : Tem.cpp
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2017-09-17
 * Last update: 2017-09-17
 * ----------------------------------------------------------------------------
 * Description:
 * Class for interfacing to Tem Driver.
 * TODO
 *    Add lock in accept to make sure we can handle situation where close 
 *    occurs while a frameAccept or frameRequest
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
#include <rogue/hardware/exo/Tem.h>
#include <rogue/hardware/exo/Info.h>
#include <rogue/hardware/exo/PciStatus.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/interfaces/stream/Buffer.h>
#include <boost/make_shared.hpp>

namespace rhe = rogue::hardware::exo;
namespace ris = rogue::interfaces::stream;
namespace bp  = boost::python;

//! Class creation
rhe::TemPtr rhe::Tem::create () {
   rhe::TemPtr r = boost::make_shared<rhe::Tem>();
   return(r);
}

//! Creator
rhe::Tem::Tem() {
   fd_      = -1;
   isData_  = false;
}

//! Destructor
rhe::Tem::~Tem() {
   this->close();
}

//! Open the device. Pass lane & vc.
bool rhe::Tem::intOpen ( std::string path, bool data ) {

   if ( fd_ > 0 ) return(false);
   isData_ = data;

   if ( (fd_ = ::open(path.c_str(), O_RDWR)) < 0 ) return(false);

   if ( isData_ ) {
      if ( temEnableDataRead(fd_) < 0 ) {
         ::close(fd_);
         return(false);
      }
   } else {
      if ( temEnableCmdRead(fd_) < 0 ) {
         ::close(fd_);
         return(false);
      }
   }

   // Start read thread
   thread_ = new boost::thread(boost::bind(&rhe::Tem::runThread, this));

   return(true);
}

//! Close the device
void rhe::Tem::close() {

   if ( fd_ < 0 ) return;

   // Stop read thread
   thread_->interrupt();
   thread_->join();
   delete thread_;
   thread_ = NULL;

   ::close(fd_);

   fd_      = -1;
   isData_  = false;
}

//! Set timeout for frame transmits in microseconds
void rhe::Tem::setTimeout(uint32_t timeout) {
   timeout_ = timeout;
}

//! Get card info.
rhe::InfoPtr rhe::Tem::getInfo() {
   rhe::InfoPtr r = rhe::Info::create();

   if ( fd_ >= 0 ) temGetInfo(fd_,r.get());
   return(r);
}

//! Get pci status.
rhe::PciStatusPtr rhe::Tem::getPciStatus() {
   rhe::PciStatusPtr r = rhe::PciStatus::create();

   if ( fd_ >= 0 ) temGetPci(fd_,r.get());
   return(r);
}

//! Accept a frame from master
bool rhe::Tem::acceptFrame ( ris::FramePtr frame ) {
   ris::BufferPtr   buff;
   int32_t          res;
   fd_set           fds;
   struct timeval   tout;
   struct timeval * tpr;
   bool             ret;

   ret = true;

   // Device is closed or buffer is empty, writes not allowed for data
   if ( (fd_ < 0) || isData_ || (frame->getPayload() == 0) ) return(false);

   buff = frame->getBuffer(0);

   // Keep trying since select call can fire 
   // but write fails because we did not win the buffer lock
   do {

      // Setup fds for select call
      FD_ZERO(&fds);
      FD_SET(fd_,&fds);

      // Setup select timeout
      if ( timeout_ > 0 ) {
         tout.tv_sec=timeout_ / 1000000;
         tout.tv_usec=timeout_ % 1000000;
         tpr = &tout;
      }
      else tpr = NULL;

      res = select(fd_+1,NULL,&fds,NULL,tpr);
      
      // Timeout
      if ( res == 0 ) {
         ret = false;
         break;
      }

      // Write
      res = temWriteCmd(fd_, buff->getRawData(), buff->getCount());

      // Error
      if ( res < 0 ) {
         ret = false;
         break;
      }
   }

   // Exit out if return flag was set false
   while ( res == 0 && ret == true );

   return(ret);
}

//! Run thread
void rhe::Tem::runThread() {
   ris::BufferPtr buff;
   ris::FramePtr  frame;
   fd_set         fds;
   int32_t        res;
   struct timeval tout;

   try {
      while(1) {

         // Setup fds for select call
         FD_ZERO(&fds);
         FD_SET(fd_,&fds);

         // Setup select timeout
         tout.tv_sec  = 0;
         tout.tv_usec = 100;

         // Select returns with available buffer
         if ( select(fd_+1,&fds,NULL,NULL,&tout) > 0 ) {

            // Allocate frame
            frame = createFrame(1024*1024*2,1024*1024*2,false,false);
            buff = frame->getBuffer(0);

            // Attempt read, lane and vc not needed since only one lane/vc is open
            res = temRead(fd_, buff->getRawData(), buff->getRawSize());

            // Read was successfull
            if ( res > 0 ) {
               buff->setSize(res);
               sendFrame(frame);
               frame.reset();
            }
         }
      }
   } catch (boost::thread_interrupted&) { }
}

void rhe::Tem::setup_python () {

   bp::class_<rhe::Tem, bp::bases<ris::Master,ris::Slave>, rhe::TemPtr, boost::noncopyable >("Tem",bp::init<>())
      .def("create",         &rhe::Tem::create)
      .staticmethod("create")
      .def("close",          &rhe::Tem::close)
      .def("getInfo",        &rhe::Tem::getInfo)
      .def("getPciStatus",   &rhe::Tem::getPciStatus)
   ;

}
