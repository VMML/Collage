
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

#ifndef CO_LATENCY_AWAREDISTRIBUTOR_H
#define CO_LATENCY_AWAREDISTRIBUTOR_H

#include "packageDistributor.h" // base class

#include <co/types.h>

namespace co
{
namespace detail { class LatencyAwareDistributor; }

class LatencyAwareDistributor : public PackageDistributor
{
public:
    /** Construct a distributor that pushes packets latency-awarely to all clients. @version 1.x */
    CO_API LatencyAwareDistributor( Producer& producer );

    /** Destruct this distributor. @version 1.x */
    virtual CO_API ~LatencyAwareDistributor();
    
    CO_API void pushItem( QueueItem &item );

private:
    detail::LatencyAwareDistributor* const _impl;
};

} // co

#endif // CO_LATENCY_AWAREDISTRIBUTOR_H
