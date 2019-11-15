/**
 *-----------------------------------------------------------------------------
 * Title      : Stream memory pool
 * ----------------------------------------------------------------------------
 * File       : Pool.h
 * Created    : 2016-09-16
 * ----------------------------------------------------------------------------
 * Description:
 * Stream memory pool
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
#ifndef __ROGUE_ENABLE_SHARED_FROM_THIS_H__
#define __ROGUE_ENABLE_SHARED_FROM_THIS_H__
#include <memory>

namespace rogue {

   class EnableSharedFromThisBase : public std::enable_shared_from_this<rogue::EnableSharedFromThisBase> {
      public:
         virtual ~EnableSharedFromThisBase() {}
   };

   template<typename T>
   class EnableSharedFromThis : virtual public EnableSharedFromThisBase {
      public:
         std::shared_ptr<T> shared_from_this() {
            return std::dynamic_pointer_cast<T>(EnableSharedFromThisBase::shared_from_this());
         }
   };
}

#endif

