
/* Copyright (c) 2014, David Steiner <steiner@ifi.uzh.ch>
 *
 * This file is part of Collage <https://github.com/Eyescale/Collage>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CO_LOADAWAREDISTRIBUTOR_H
#define CO_LOADAWAREDISTRIBUTOR_H

#include "packageDistributor.h" // base class

#include <co/types.h>

namespace co
{
namespace detail { class LoadAwareDistributor; }

class LoadAwareDistributor : public PackageDistributor
{
public:
    static const int32_t FEEDBACK_INTERVAL;
    
    static const int32_t ITEMS_TO_SEND;
    
    /** Construct a random distributor. @version 1.x */
    CO_API LoadAwareDistributor( Producer& producer );

    /** Destruct this distributor. @version 1.x */
    virtual CO_API ~LoadAwareDistributor();
    
    CO_API void pushItem( QueueItem &item );
    
    CO_API void cmdSlaveFeedback( co::ICommand& comd );
    
    CO_API void notifyQueueEnd();

private:
    detail::LoadAwareDistributor* const _impl;
};

} // co

#endif // CO_LOADAWAREDISTRIBUTOR_H
