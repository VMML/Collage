
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

#ifndef CO_CONSUMER_H
#define CO_CONSUMER_H

#include <co/api.h>
#include <co/types.h>
#include <co/object.h> // base class
#include <co/slaveFeedback.h>
#include <co/objectICommand.h>

namespace co
{

/**
 * Some form of consumer, usually a queue
 */
class Consumer : public Object
{
public:

    enum Flags
    {
        FLAG_QUEUE_RIGHT = 1,
    };

    /**
     * Pop an item.
     *
     * The returned item can deserialize additional data using the DataIStream
     * operators.
     *
     * @param timeout An optional timeout for the operation.
     * @return an item from the distributed queue, or an invalid item if the
     *         queue is empty or the operation timed out.
     * @version 1.0
     */
    virtual ObjectICommand pop( const uint32_t timeout = LB_TIMEOUT_INDEFINITE ) = 0;

    /**
     * Pop an item.
     *
     * The returned item can deserialize additional data using the DataIStream
     * operators.
     *
     * @param timeout timeout for the operation.
     * @param flags may be set by the operation.
     * @return an item from the distributed queue, or an invalid item if the
     *         queue is empty or the operation timed out.
     * @version 1.0
     */
    virtual ObjectICommand pop( const uint32_t timeout, uint32_t )
    {
        return pop( timeout );
    }
    
    /**
     * Send feedback to the master queue.
     *
     * @version 1.x
     */
    virtual SlaveFeedback sendSlaveFeedback() = 0;

    /**
     * Set score of this slave queue.
     *
     * @version 1.x
     */
    virtual void setScore(float score) = 0;

    /**
     * Set score of this slave queue.
     *
     * @version 1.x
     */
    virtual float getScore() = 0;

    /**
     * Reset score of this slave queue.
     *
     * @version 1.x
     */
    virtual void resetScore() = 0;
};

} // co

#endif // CO_CONSUMER_H
