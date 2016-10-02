/**
 *-----------------------------------------------------------------------------
 * Title         : Data file reader utility.
 * ----------------------------------------------------------------------------
 * File          : StreamReader.cpp
 * Author        : Ryan Herbst <rherbst@slac.stanford.edu>
 * Created       : 09/28/2016
 * Last update   : 09/28/2016
 *-----------------------------------------------------------------------------
 * Description :
 *    Class to read data files.
 *-----------------------------------------------------------------------------
 * This file is part of the rogue software platform. It is subject to 
 * the license terms in the LICENSE.txt file found in the top-level directory 
 * of this distribution and at: 
    * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
 * No part of the rogue software platform, including this file, may be 
 * copied, modified, propagated, or distributed except according to the terms 
 * contained in the LICENSE.txt file.
 *-----------------------------------------------------------------------------
**/
#include <rogue/utilities/fileio/StreamReader.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/exceptions/OpenException.h>
#include <rogue/exceptions/AllocationException.h>
#include <stdint.h>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <fcntl.h>

namespace ris = rogue::interfaces::stream;
namespace ruf = rogue::utilities::fileio;
namespace bp  = boost::python;
namespace re  = rogue::exceptions;

//! Class creation
ruf::StreamReaderPtr ruf::StreamReader::create () {
   ruf::StreamReaderPtr s = boost::make_shared<ruf::StreamReader>();
   return(s);
}

//! Setup class in python
void ruf::StreamReader::setup_python() {
   bp::class_<ruf::StreamReader, ruf::StreamReaderPtr,bp::bases<ris::Master>, boost::noncopyable >("StreamReader",bp::init<>())
      .def("create",         &ruf::StreamReader::create)
      .staticmethod("create")
      .def("open",           &ruf::StreamReader::open)
      .def("close",          &ruf::StreamReader::close)
   ;
}

//! Creator
ruf::StreamReader::StreamReader() { 
   baseName_   = "";
   readThread_ = NULL;
}

//! Deconstructor
ruf::StreamReader::~StreamReader() { 
   close();
}

//! Open a data file
void ruf::StreamReader::open(std::string file) {
   close();

   // Determine if we read a group of files
   if ( file.substr(file.find_last_of(".")) == ".1" ) {
      fdIdx_ = 1;
      baseName_ = file.substr(0,file.find_last_of("."));
   }
   else {
      fdIdx_ = 0;
      baseName_ = file;
   }

   if ( (fd_ = ::open(file.c_str(),O_RDONLY)) < 0 ) 
      throw(re::OpenException("StreamReader::open",file,0));

   readThread_ = new boost::thread(boost::bind(&StreamReader::runThread, this));
}

//! Open file
bool ruf::StreamReader::nextFile() {
   std::string name;

   if ( fd_ >= 0 ) {
      ::close(fd_);
      fd_ = -1;
   } else return(false);

   if ( fdIdx_ == 0 ) return(false);

   fdIdx_++;
   name = baseName_ + "." + boost::lexical_cast<std::string>(fdIdx_);

   if ( (fd_ = ::open(name.c_str(),O_RDONLY)) < 0 ) return(false);
   return(true);
}

//! Close a data file
void ruf::StreamReader::close() {
   printf("Closing\n");
   if ( readThread_ != NULL ) {
      printf("Interrupt Thread\n");
      readThread_->interrupt();
      printf("Join Thread\n");
      readThread_->join();
      delete readThread_;
      readThread_ = NULL;
   }
   printf("Done\n");
}

//! Thread background
void ruf::StreamReader::runThread() {
   int32_t  ret;
   uint32_t size;
   uint32_t flags;
   ris::FramePtr frame;
   ris::FrameIteratorPtr iter;

   try {
      do {

         // Read size of each frame
         while ( fd_ > 0 && read(fd_,&size,4) == 4 ) {
            if ( size == 0 ) {
               printf("StreamReader::runThread -> Bad size read %i",size);
               ::close(fd_);
               fd_ = -1;
               return;
            }

            // Read flags
            if ( read(fd_,&flags,4) != 4 ) {
               printf("StreamReader::runThread -> Failed to read flags");
               ::close(fd_);
               fd_ = -1;
               return;
            }

            // Request frame
            frame = reqFrame(size,true,0);
            frame->setFlags(flags);

            // Populate frame
            iter = frame->startWrite(0,size-4);
            do {
               if ( (ret = read(fd_,iter->data(),iter->size())) != (int32_t)iter->size()) {
                  printf("StreamReader::runThread -> Short read. Ret = %i Req = %i after %i bytes",ret,iter->size(),iter->total());
                  ::close(fd_);
                  fd_ = -1;
                  frame->setError(0x1);
               }
            } while (frame->nextWrite(iter));
            sendFrame(frame);
         }
      } while ( nextFile() );
   } catch (boost::thread_interrupted&) {}
}

