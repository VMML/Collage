
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

#ifndef CO_PRODUCER_H
#define CO_PRODUCER_H

#include <co/object.h> // base class
#include <co/types.h>
#include <co/packageDistributor.h>

namespace co
{

/**
 * Some form of producer, usually a queue
 */
class Producer : public Object
{
public:

    /**
     * Set distribution strategy for this producer.
     *
     * @version 1.x
     */
    virtual CO_API void setPackageDistributor( PackageDistributorPtr distributor ) { _distributor = distributor; }

    /**
     * Get this producer distribution strategy.
     *
     * @version 1.x
     */
    virtual CO_API PackageDistributorPtr getPackageDistributor() { return _distributor; }

    /**
     * Push an item.
     *
     * The returned queue item can stream additional data using the DataOStream
     * operators. Note that the item is enqueued once it is destroyed, i.e. when
     * it runs out of scope.
     *
     * @return the item to enqueue.
     * @version 1.0
     */
    virtual CO_API QueueItem push() = 0;

    /** Remove all enqueued items. @version 1.0 */
    virtual CO_API void clear() = 0;
    
    virtual void _addItem( QueueItem& item ) = 0;   // HACK

private:
    PackageDistributorPtr _distributor;
};

} // co

#endif // CO_PRODUCER_H
