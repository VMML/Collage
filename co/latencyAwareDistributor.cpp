
/* Copyright (c) 2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "latencyAwareDistributor.h"
#include "stealingQueueCommand.h"

#include <co/objectOCommand.h>
#include <co/queueItem.h>
#include <co/producer.h>

#include <lunchbox/array.h>
#include <lunchbox/buffer.h>

namespace co
{

namespace detail
{

class LatencyAwareDistributor
{
public:
    LatencyAwareDistributor( co::LatencyAwareDistributor& parent, Producer& producer )
        : _parent( parent )
        , _producer( producer )
        , _index( 0 )
    {}

    co::LatencyAwareDistributor& _parent;
    co::Producer& _producer;
    size_t _index;
};

}

LatencyAwareDistributor::LatencyAwareDistributor( Producer& producer )
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::LatencyAwareDistributor( *this, producer ) )
#pragma warning(pop)
{
}

LatencyAwareDistributor::~LatencyAwareDistributor()
{
    delete _impl;
}

void LatencyAwareDistributor::pushItem( QueueItem &item )
{
    LocalNodePtr localNode = _impl->_producer.getLocalNode();
    if (localNode)
    {
        float pos = item.getPositionHint();
        std::cout << "Sending item (position: " << pos << ") . . ." << std::endl;

        Nodes nodes;
        localNode->getNodes( nodes, false );
        int nNodes = nodes.size();
        float absPos = nNodes * pos;    // map to node

        NodePtr node = nodes[absPos];
        lunchbox::Bufferb &buffer = item.getBuffer();
        ConnectionPtr conn = node->getConnection();
        co::Connection::RoundTripTime rtt = conn->getRTT();
        

        std::cout << "LatencyAware MASTER (" << localNode->getNodeID() << "): pushing task to node " << node->getNodeID() << " . . .\n"
                 << "RTT: " << rtt << std::endl;    // TEST
        _impl->_producer.send( node, CMD_QUEUE_ITEM_LEFT )
            << Array< const void >( buffer.getData(), buffer.getSize( ));
    }
}

} // co
