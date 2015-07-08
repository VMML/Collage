
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

#include "randomDistributor.h"
#include "stealingQueueCommand.h"

#include <co/objectICommand.h>
#include <co/queueCommand.h>
#include <co/objectOCommand.h>
#include <co/queueItem.h>
#include <co/producer.h>
#include <lunchbox/array.h>

#include <lunchbox/buffer.h>
#include <lunchbox/mtQueue.h>

namespace co
{

namespace detail
{

class ItemBuffer : public lunchbox::Bufferb, public lunchbox::Referenced
{
public:
    ItemBuffer( lunchbox::Bufferb& from )
        : lunchbox::Bufferb( from )
        , lunchbox::Referenced()
    {}

    ~ItemBuffer()
    {}
};

typedef lunchbox::RefPtr< ItemBuffer > ItemBufferPtr;
typedef std::vector< detail::ItemBufferPtr > Items;

class RandomDistributor
{
public:
    typedef lunchbox::MTQueue< ItemBufferPtr > ItemQueue;
    
    RandomDistributor( co::RandomDistributor& parent, Producer& producer )
        : _parent( parent )
        , _producer( producer )
    {}

    co::RandomDistributor& _parent;
    co::Producer& _producer;
    ItemQueue _queue;
};

}

RandomDistributor::RandomDistributor( Producer& producer )
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::RandomDistributor( *this, producer ) )
#pragma warning(pop)
{
}

RandomDistributor::~RandomDistributor()
{
    delete _impl;
}

void RandomDistributor::pushItem( QueueItem &item )
{
    detail::ItemBufferPtr newBuffer = new detail::ItemBuffer( item.getBuffer());
    _impl->_queue.push( newBuffer );
}

bool RandomDistributor::cmdGetItem( co::ICommand& comd )
{
    co::ObjectICommand command( comd );

    const uint32_t itemsRequested = command.get< uint32_t >();
    const float score LB_UNUSED = command.get< float >();
    const uint32_t slaveInstanceID = command.get< uint32_t >();
    const int32_t requestID = command.get< int32_t >();

    detail::Items items;
    _impl->_queue.tryPop( itemsRequested, items );

    for( detail::Items::const_iterator i = items.begin(); i != items.end(); ++i )
    {
        const detail::ItemBufferPtr item = *i;
        if( !item->isEmpty( ))
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID ) << false
                << Array< const void >( item->getData(), item->getSize( ));
    }

    if( itemsRequested > items.size( ))
        _impl->_producer.send( command.getNode(), CMD_QUEUE_EMPTY, slaveInstanceID ) << requestID;

    return true;
}

} // co
