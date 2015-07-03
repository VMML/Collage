
/* Copyright (c) 2014-2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "equalDistributor.h"
#include "stealingQueueCommand.h"

#include <co/objectICommand.h>
#include <co/queueCommand.h>
#include <co/objectOCommand.h>
#include <co/queueItem.h>
#include <co/producer.h>
#include <lunchbox/array.h>

#include <lunchbox/buffer.h>
#include <lunchbox/floatMap.h>
#include <lunchbox/mtQueue.h>

#include <boost/shared_ptr.hpp>

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

class EqualDistributor
{
public:
    typedef std::map< NodeID, int > NodeMap;
    typedef lunchbox::MTQueue< ItemBufferPtr > ItemQueue;
    typedef boost::shared_ptr< ItemQueue > ItemQueuePtr;
    typedef std::vector< ItemQueuePtr > ItemQueues;
    
    EqualDistributor( co::EqualDistributor& parent, Producer& producer )
        : _parent( parent )
        , _producer( producer )
        , _nNodes( 0 )
    {}

    Nodes _nodes;
    NodeMap _nodeMap;
    ItemQueues _queues;
    co::EqualDistributor& _parent;
    co::Producer& _producer;
    size_t _nNodes;
};

}

EqualDistributor::EqualDistributor( Producer& producer )
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::EqualDistributor( *this, producer ) )
#pragma warning(pop)
{
}

EqualDistributor::~EqualDistributor()
{
    delete _impl;
}

void EqualDistributor::pushItem( QueueItem &item )
{
    size_t nNodes = _impl->_nodeMap.size();
    if( nNodes < 1 )
    {
        _impl->_queues.resize( _impl->_nNodes );
        for( size_t i = 0; i < _impl->_nNodes; ++i )
        {
            _impl->_nodeMap[ _impl->_nodes[ i ]->getNodeID() ] = i;
            _impl->_queues[ i ] = detail::EqualDistributor::ItemQueuePtr(
                new detail::EqualDistributor::ItemQueue );
        }
    }

    float pos = item.getPositionHint();
    int absPos = _impl->_nNodes * pos;    // map to node

//    LBWARN << "Pushing item (position: " << pos << ") to queue " << absPos << " . . ." << std::endl;
    detail::ItemBufferPtr newBuffer = new detail::ItemBuffer( item.getBuffer( ));
    _impl->_queues[absPos]->push( newBuffer );
}

bool EqualDistributor::cmdGetItem( co::ICommand& comd )
{
    NodeID nodeID = comd.getNode()->getNodeID();

    co::ObjectICommand command( comd );
    const uint32_t itemsRequested = command.get< uint32_t >();
    const float score LB_UNUSED = command.get< float >();
    const uint32_t slaveInstanceID = command.get< uint32_t >();
    const int32_t requestID = command.get< int32_t >();

    int absPos = _impl->_nodeMap[ nodeID ];
//    LBWARN << "Popping item from queue " << absPos << " . . ." << std::endl;

    detail::Items items;
    _impl->_queues[ absPos ]->tryPop( itemsRequested, items );

    for( detail::Items::const_iterator i = items.begin(); i != items.end(); ++i )
    {
        const detail::ItemBufferPtr item = *i;
        if( !item->isEmpty( ))
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID )
                << Array< const void >( item->getData(), item->getSize( ));
    }

    if( itemsRequested > items.size( ))
        _impl->_producer.send( command.getNode(), CMD_QUEUE_EMPTY, slaveInstanceID ) << requestID;

    return true;
}

void EqualDistributor::setSlaveNodes(const co::Nodes& nodes)
{
    _impl->_nodes = nodes;
    _impl->_nNodes = nodes.size();
}

} // co
