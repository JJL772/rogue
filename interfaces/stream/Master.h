/**
 *-----------------------------------------------------------------------------
 * Title      : Stream interface master
 * ----------------------------------------------------------------------------
 * File       : Master.h
 * Author     : Ryan Herbst, rherbst@slac.stanford.edu
 * Created    : 2016-08-08
 * Last update: 2016-08-08
 * ----------------------------------------------------------------------------
 * Description:
 * Stream interface master
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
#ifndef __INTERFACES_STREAM_MASTER_H__
#define __INTERFACES_STREAM_MASTER_H__
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <string>
#include <vector>

#include <boost/python.hpp>

namespace interfaces {
   namespace stream {

   class Slave;
   class Frame;

      //! Stream master class
      /*
       * This class pushes frame data to a slave interface.
       */
      class Master {
            boost::shared_ptr<interfaces::stream::Slave> primary_;
            std::vector<boost::shared_ptr<interfaces::stream::Slave> > slaves_;

         public:

            //! Class creation
            static boost::shared_ptr<interfaces::stream::Master> create ();

            //! Creator
            Master();

            //! Destructor
            virtual ~Master();

            //! Set primary slave, used for buffer request forwarding
            void setSlave ( boost::shared_ptr<interfaces::stream::Slave> slave );

            //! Add secondary slave
            void addSlave ( boost::shared_ptr<interfaces::stream::Slave> slave );

            //! Get frame from slave
            /*
             * An allocate command will be issued to the primary slave set with setSlave()
             */
            boost::shared_ptr<interfaces::stream::Frame>
               reqFrame ( uint32_t size, bool zeroCopyEn);

            //! Push frame to all slaves
            bool sendFrame ( boost::shared_ptr<interfaces::stream::Frame> frame );
      };

      // Convienence
      typedef boost::shared_ptr<interfaces::stream::Master> MasterPtr;
   }
}

#endif
