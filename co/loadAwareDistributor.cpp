
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

#include "loadAwareDistributor.h"
#include "stealingQueueCommand.h"

#include <co/objectICommand.h>
#include <co/objectOCommand.h>
#include <co/queueItem.h>
#include <co/producer.h>
#include <lunchbox/array.h>
#include <lunchbox/referenced.h>
#include <boost/concept_check.hpp>

#include <lunchbox/scopedMutex.h>
#include <lunchbox/referenced.h>
#include <lunchbox/lockable.h>
#include <lunchbox/buffer.h>
#include <lunchbox/clock.h>

#include <boost/tuple/tuple.hpp>

#include <vector>
#include <cmath>

// TEST
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

namespace co
{

const int32_t LoadAwareDistributor::FEEDBACK_INTERVAL   = 25;
const int32_t LoadAwareDistributor::ITEMS_TO_SEND       = 25;   // TEST

namespace detail
{

class LoadAwareDistributor
{
public:
    typedef std::vector< int64_t > DrawTimes;
    typedef boost::tuple< int, bool > Mapping;
    typedef lunchbox::Lockable< int > ItemCounter;

    class SlaveInfo : public lunchbox::Referenced
    {
    public:
        SlaveInfo()
            : itemsToSend( co::LoadAwareDistributor::ITEMS_TO_SEND )
            , drawTimes( 2 )
            , tasksSent( 0 )
            , starving( false )
        {}

        ItemCounter itemsToSend;
        DrawTimes drawTimes;
        uint32_t tasksSent;
        uint32_t tasksDone;
        bool starving;
    };
    typedef lunchbox::RefPtr< SlaveInfo > SlaveInfoPtr;
    typedef std::map< NodeID, SlaveInfoPtr> SlaveInfos;

    LoadAwareDistributor( co::LoadAwareDistributor& parent, Producer& producer )
        : _parent( parent )
        , _producer( producer )
    {}
    
    Mapping adjustMapping( const Nodes &nodes, int nodeIndex, bool right );
    
    SlaveInfoPtr getSlaveInfo( const NodeID &nodeID );

    co::LoadAwareDistributor& _parent;
    co::Producer& _producer;
    SlaveInfos _slaveInfos;
};

LoadAwareDistributor::SlaveInfoPtr LoadAwareDistributor::getSlaveInfo( const NodeID &nodeID )
{
    SlaveInfos::iterator find = _slaveInfos.find( nodeID );
    if (find == _slaveInfos.end())
    {
        SlaveInfoPtr slaveInfo = new detail::LoadAwareDistributor::SlaveInfo;
        _slaveInfos[nodeID] = slaveInfo;
        return slaveInfo;
    }
    return find->second;
}

LoadAwareDistributor::Mapping LoadAwareDistributor::adjustMapping( const Nodes &nodes, int nodeIndex, bool right )
{
    uint32_t minTime = ~0;
    bool firstRun = true;
    bool nodeQueue = right;
    for (size_t i = 0; i < 2; ++i)
    {
        int index = (nodeIndex + i + (right ? 0 : -1)) % nodes.size();
        NodeID nodeID = nodes[index]->getNodeID();
        detail::LoadAwareDistributor::SlaveInfoPtr slaveInfo = getSlaveInfo(nodeID);

        bool queue = (index == nodeIndex) ?  // select opposite queue for neighboring node
                      right               :
                     !right;

        int64_t drawTime = slaveInfo->drawTimes[queue];
//         std::cout << "drawTime: " << drawTime << ", minTime: " << minTime << std::endl;
        if (drawTime < minTime)
        {
            minTime = drawTime;
            if (!firstRun)
            {
                std::cout << "Adjusting index " << nodeIndex << " => " << index
                          << ", queue " << nodeQueue << " => " << queue << std::endl;

                nodeIndex = index;
                nodeQueue = queue;
            }
            firstRun = false;
        }
    }
    
    return Mapping(nodeIndex, nodeQueue);
}

}

LoadAwareDistributor::LoadAwareDistributor( Producer& producer )
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::LoadAwareDistributor( *this, producer ) )
#pragma warning(pop)
{
}

LoadAwareDistributor::~LoadAwareDistributor()
{
    delete _impl;
}

void LoadAwareDistributor::notifyQueueEnd()
{
    std::cout << "LoadAwareDistributor::notifyQueueEnd . . ." << std::endl;
    
    if (_impl->_slaveInfos.size() > 0)
    {
        for (detail::LoadAwareDistributor::SlaveInfos::iterator it = _impl->_slaveInfos.begin();
                it != _impl->_slaveInfos.end();
                ++it)
        {
            detail::LoadAwareDistributor::ItemCounter &itemCounter = it->second->itemsToSend;
            *itemCounter = co::LoadAwareDistributor::ITEMS_TO_SEND;
        }
    }
}

void LoadAwareDistributor::pushItem( QueueItem &item )
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
        int nodeIndex = absPos;
        bool right = (absPos > (float(nodeIndex) + .5f));   // whether to map to left or right queue

        while (true)
        {
            int limit = ~0 / 2;     // TEST
            if (_impl->_slaveInfos.size() > 1)
            {
                int avg = 0;
                int size = _impl->_slaveInfos.size();
                for (detail::LoadAwareDistributor::SlaveInfos::const_iterator it = _impl->_slaveInfos.begin();
                    it != _impl->_slaveInfos.end();
                    ++it)
                {
                    const detail::LoadAwareDistributor::ItemCounter &itemCounter = it->second->itemsToSend;
                    lunchbox::ScopedMutex<> mutex(itemCounter);
                    avg += *itemCounter;
                }
                avg /= size;
                
                int var = 0;
                for (detail::LoadAwareDistributor::SlaveInfos::const_iterator it = _impl->_slaveInfos.begin();
                    it != _impl->_slaveInfos.end();
                    ++it)
                {
                    const detail::LoadAwareDistributor::ItemCounter &itemCounter = it->second->itemsToSend;
                    lunchbox::ScopedMutex<> mutex(itemCounter);
                    int dist = avg - *itemCounter;
                    
                    var += dist * dist;
                }
                var /= (size - 1);
                limit = avg + 2 * std::sqrt(var);
            }
            std::cout << "limit: " << limit << std::endl;

            // find nearest starving node, if there is any
            for (int j = 0; j < nNodes; ++j)
            {
                for (int i = -1; i < 2; ++i)
                {
                    int index = nodeIndex + j * i;
                    if (index >= 0 && index < nNodes)
                    {
                        NodePtr node = nodes[index];
                        NodeID nodeID = node->getNodeID();

                        detail::LoadAwareDistributor::SlaveInfoPtr slaveInfo = _impl->getSlaveInfo(nodeID);
                        if (*slaveInfo->itemsToSend > limit)     // TEST
                        {
                            std::cout << "Found starving node at index " << index << ", adjusting nodeIndex from " << nodeIndex << " to " << index << " . . ." << std::endl;
                            nodeIndex = index;
                            right = (slaveInfo->drawTimes[true] < slaveInfo->drawTimes[false]);
                            break;
                        }
                    }
                }
            }
            
            detail::LoadAwareDistributor::Mapping mapping( _impl->adjustMapping( nodes, nodeIndex, right ) );
            nodeIndex = mapping.get< 0 >();
            bool nodeQueue = mapping.get< 1 >();
            
            NodePtr node = nodes[nodeIndex];
            NodeID nodeID = node->getNodeID();
            detail::LoadAwareDistributor::SlaveInfoPtr slaveInfo = _impl->_slaveInfos[nodeID];
            detail::LoadAwareDistributor::ItemCounter &itemCounter = slaveInfo->itemsToSend;
            lunchbox::ScopedMutex<> mutex( itemCounter );
            
            std::cout << "itemCounter: " << *itemCounter << std::endl;
            if (*itemCounter >= 0)
            {
                std::cout << "MASTER (" << localNode->getNodeID() << "): pushing task to node " << node->getNodeID() << ": index, " << nodeIndex << ", queue: " << nodeQueue << ", absPos is " << absPos << " . . ." << std::endl;

                lunchbox::Bufferb &buffer = item.getBuffer();
                bool pushRight = (nodeQueue > 0);
                if (pushRight)
                {
                    _impl->_producer.send( node, CMD_QUEUE_ITEM_RIGHT )
                            << Array< const void >( buffer.getData(), buffer.getSize( ));
                }
                else
                {
                    _impl->_producer.send( node, CMD_QUEUE_ITEM_LEFT )
                            << Array< const void >( buffer.getData(), buffer.getSize( ));
                }
                ++slaveInfo->tasksSent;
                slaveInfo->starving = false;

                std::cout << localNode->getNodeID() << " _impl->_itemsToSend: " << *itemCounter << std::endl;
                --*itemCounter;

                break;
            }
        }
    }
}

void LoadAwareDistributor::cmdSlaveFeedback( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
    NodePtr node = command.getNode();
    NodeID nodeID = node->getNodeID();

    bool starving;
    command >> starving;
    int64_t time;
    command >> time;
    bool right;
    command >> right;
    
    detail::LoadAwareDistributor::SlaveInfoPtr slaveInfo = _impl->_slaveInfos[nodeID];
    if (starving)
    {
        std::cout << "LoadAwareDistributor::cmdSlaveFeedback, node " << nodeID << " is starving . . ." << std::endl;
        slaveInfo->starving = true;
        
        detail::LoadAwareDistributor::ItemCounter &itemCounter = slaveInfo->itemsToSend;
        {
            lunchbox::ScopedMutex<> mutex( itemCounter );
            *itemCounter += ITEMS_TO_SEND;
        }
        return;
    }
    
    slaveInfo->drawTimes[right] += time;
    std::cout << "LoadAwareDistributor::cmdSlaveFeedback, node: " << nodeID << ", time: " << time << ", right: " << right << std::endl;
}

} // co
 
