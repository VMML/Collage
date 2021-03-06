
/* Copyright (c) 2011-2013, Stefan Eilemann <eile@eyescale.ch>
 *                    2011, Carsten Rohn <carsten.rohn@rtt.ag>
 *               2011-2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *                    2013, David Steiner <steiner@ifi.uzh.ch>
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

#ifndef CO_STEALINGQUEUEMASTER_H
#define CO_STEALINGQUEUEMASTER_H

#include "producer.h" // base class

#include <co/types.h>

namespace co
{
namespace detail { class StealingQueueMaster; }

/**
 * The producer end of a distributed queue.
 *
 * One instance of this class is registered with a LocalNode to for the producer
 * end of a distributed queue. One or more QueueSlave instances are mapped to
 * this master instance and consume the data.
 */
class StealingQueueMaster : public Producer
{
public:
    /** Construct a new queue master. @version 1.0 */
    CO_API StealingQueueMaster();

    /** Destruct this queue master. @version 1.0 */
    virtual CO_API ~StealingQueueMaster();

    /**
     * Set distribution strategy for this queue.
     *
     * @version 1.x
     */
    CO_API void setPackageDistributor( PackageDistributorPtr distributor );

    /**
     * Get this queues distribution strategy.
     *
     * @version 1.x
     */
    CO_API PackageDistributorPtr getPackageDistributor();

    /**
     * Enqueue a new queue item.
     *
     * The returned queue item can stream additional data using the DataOStream
     * operators. Note that the item is enqueued once it is destroyed, i.e. when
     * it runs out of scope.
     *
     * @return the item to enqueue.
     * @version 1.0
     */
    CO_API QueueItem push();

    /** Remove all enqueued items. @version 1.0 */
    CO_API void clear();

private:
    detail::StealingQueueMaster* const _impl;

    CO_API void attach( const uint128_t& id, const uint32_t instanceID ) override;

    ChangeType getChangeType() const override { return STATIC; }
    void getInstanceData( co::DataOStream& os ) override;
    void applyInstanceData( co::DataIStream& ) override { LBDONTCALL }

    void _addItem( QueueItem& item );
};

} // co

#endif // CO_STEALINGQUEUEMASTER_H
