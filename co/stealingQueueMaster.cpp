
/* Copyright (c) 2011-2012, Stefan Eilemann <eile@eyescale.ch>
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

#include "stealingQueueMaster.h"

#include "queueItem.h"
#include "dataOStream.h"
#include "objectICommand.h"
#include "objectOCommand.h"
#include "packageDistributor.h"
#include "connectionDescription.h"
#include "stealingQueueCommand.h"

#include <lunchbox/mtQueue.h>
#include <lunchbox/buffer.h>
#include <lunchbox/clock.h>

#include <co/slaveFeedback.h>

#include <limits>
#include <string>
#include <sstream>
#include <utility>

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

class StealingQueueMaster : public co::Dispatcher
{
public:
    StealingQueueMaster( co::StealingQueueMaster& parent )
        : co::Dispatcher()
        , _parent( parent )
        , _nNodes(0)
    {}
    
    bool cmdStealItem( co::ICommand& comd );
    
    bool cmdSlaveFeedback( co::ICommand& comd );
    
    bool cmdGetItem( co::ICommand& comd );

    void updateNodeInfo();
    
    void setupLoad();

    typedef lunchbox::MTQueue< ItemBufferPtr > ItemQueue;
    typedef std::pair<int, int> Load;

    ItemQueue queue;
    co::StealingQueueMaster& _parent;
    size_t _nNodes;
    lunchbox::Clock _clock;
    std::vector< Load > _loads;
    PackageDistributorPtr _distributor;
};

bool StealingQueueMaster::cmdStealItem( co::ICommand& comd )
{
    co::ObjectICommand command( comd );

    /*const uint32_t ratio =*/ command.get< uint32_t >();
    const uint32_t slaveInstanceID = command.get< uint32_t >();
    const int32_t requestID = command.get< int32_t >();
    Connections connections( 1, command.getNode()->getConnection( ));

    std::cerr << "MASTER: denying steal attempt . . ." << std::endl;
    co::ObjectOCommand( connections, CMD_QUEUE_DENY_MASTER,
                        COMMANDTYPE_OBJECT, command.getObjectID(),
                        slaveInstanceID ) << requestID;
    return true;
}

bool StealingQueueMaster::cmdSlaveFeedback( co::ICommand& comd )
{
    if (_distributor)
    {
        _distributor->cmdSlaveFeedback(comd);
    }
    return true;
}

bool StealingQueueMaster::cmdGetItem( co::ICommand& comd )
{
    if (_distributor)
    {
        _distributor->cmdGetItem(comd);
    }
    return true;
}

void StealingQueueMaster::updateNodeInfo()
{
    LocalNodePtr localNode = _parent.getLocalNode();
    if (localNode)
    {
        Nodes nodes;
        localNode->getNodes(nodes, false);
        size_t nNodes = nodes.size();
        if (_nNodes != nNodes)
        {
            _nNodes = nNodes;

            int nCds = 0;
            std::stringstream msg;
            for (Nodes::const_iterator i = nodes.begin(); i != nodes.end(); ++i)
            {
                const ConnectionDescriptions& cds = (*i)->getConnectionDescriptions();
                msg << serialize(cds) << " ";
                ++nCds;
            }

            std::cout << "MASTER (" << localNode->getNodeID() << "): sending victim info to . . ." << std::endl;
            for (Nodes::const_iterator i = nodes.begin(); i != nodes.end(); ++i)
            {
                std::cout << ". . . " << (*i)->getNodeID() << std::endl;
//                     _parent.send( *i, CMD_QUEUE_HELLO );
                _parent.send( (*i), CMD_QUEUE_VICTIM_DATA ) << nCds << msg.str();
            }
        }
    }
}

void StealingQueueMaster::setupLoad()
{
    LocalNodePtr localNode = _parent.getLocalNode();
    if (localNode)
    {
        Nodes nodes;
        localNode->getNodes(nodes, false);
        size_t nNodes = nodes.size();
        if (_nNodes != nNodes)
        {
            _nNodes = nNodes;
            
            _loads.clear();
            for (size_t i = 0; i < nNodes; ++i)
            {
                
            }
        }
    }
}

}

StealingQueueMaster::StealingQueueMaster()
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::StealingQueueMaster( *this ))
#pragma warning(pop)
{
}

StealingQueueMaster::~StealingQueueMaster()
{
    clear();
    delete _impl;
}

void StealingQueueMaster::attach( const uint128_t& id, const uint32_t instanceID )
{
    Object::attach( id, instanceID );

    LocalNodePtr localNode = getLocalNode();
    CommandQueue* queue = localNode->getCommandThreadQueue();
    registerCommand( CMD_QUEUE_STEAL_ITEM,
                     CommandFunc< detail::StealingQueueMaster >(
                         _impl, &detail::StealingQueueMaster::cmdStealItem ), queue );
    registerCommand( CMD_QUEUE_SLAVE_FEEDBACK,
                     CommandFunc< detail::StealingQueueMaster >(
                         _impl, &detail::StealingQueueMaster::cmdSlaveFeedback ), queue );
    registerCommand( CMD_QUEUE_GET_ITEM,
                     CommandFunc< detail::StealingQueueMaster >(
                         _impl, &detail::StealingQueueMaster::cmdGetItem ), queue );
}

void StealingQueueMaster::clear()
{
    if (_impl->_distributor)
    {
        _impl->_distributor->clear();
    }
    _impl->queue.clear();
}

void StealingQueueMaster::getInstanceData( co::DataOStream& os )
{
    os << getInstanceID() << getLocalNode()->getNodeID();
}

QueueItem StealingQueueMaster::push()
{
    return QueueItem( *this );
}

void StealingQueueMaster::setPackageDistributor( PackageDistributorPtr distributor )
{
    Producer::setPackageDistributor( distributor );
    _impl->_distributor = distributor;
}

PackageDistributorPtr StealingQueueMaster::getPackageDistributor()
{
    return _impl->_distributor;
}

void StealingQueueMaster::_addItem( QueueItem& item )
{
//     Nodes nodes;
//     LocalNodePtr localNode = getLocalNode();
//     if (localNode)
//     {
//         localNode->getNodes(nodes, false);
// 
//         _impl->updateNodeInfo();
//         _impl->setupLoad();
//     }
//     else
//     {
//         std::cerr << "MASTER: node ID is NULL . . .";
//         return;
//     }
// 
// //     std::cout << "newBuffer->getSize(): " << item.getBuffer().getSize() << std::endl;     // TEST
//     if (item.getBuffer().getSize() < 1)
//     {
// //         std::cout << "Queue is empty (time: " << _impl->_clock.getTime64() << ") . . ." << std::endl;
//         _impl->_clock.reset();
// // #ifdef EQUALIZER_USE_WORK_STEALING
// //         for (NodesCIter i = nodes.begin(); i != nodes.end(); ++i)
// //         {
// //             send( *i, CMD_MASTER_QUEUE_EMPTY );
// //         }
// // #endif
//         if (_impl->_distributor)
//         {
//             _impl->_distributor->notifyQueueEnd();
//         }
//     }
//     else
//     {
//         LBASSERT(_impl->_distributor);
        _impl->_distributor->pushItem(item);
//     }
}

} // co
