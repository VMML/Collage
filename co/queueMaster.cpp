
/* Copyright (c) 2011-2014, Stefan Eilemann <eile@eyescale.ch>
 *                    2011, Carsten Rohn <carsten.rohn@rtt.ag>
 *               2011-2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *               2013-2014, David Steiner <steiner@ifi.uzh.ch>
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

#include "queueMaster.h"

#include "queueItem.h"
#include "dataOStream.h"
#include "objectICommand.h"
#include "objectOCommand.h"
#include "packageDistributor.h"
#include "connectionDescription.h"
#include "queueCommand.h"

#include <lunchbox/buffer.h>
#include <lunchbox/mtQueue.h>
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

class QueueMaster : public co::Dispatcher
{
public:
    QueueMaster( co::QueueMaster& parent )
        : co::Dispatcher()
        , _parent( parent )
        , _nNodes(0)
    {}

    bool cmdGetItem( co::ICommand& comd );

    void updateNodeInfo();

    void setupLoad();

    typedef lunchbox::MTQueue< ItemBufferPtr > ItemQueue;
    typedef std::pair<int, int> Load;

    ItemQueue queue;
    co::QueueMaster& _parent;
    size_t _nNodes;
    lunchbox::Clock _clock;
    std::vector< Load > _loads;
    PackageDistributorPtr _distributor;
};

bool QueueMaster::cmdGetItem( co::ICommand& comd )
{
    if (_distributor)
    {
        _distributor->cmdGetItem(comd);
    }
    return true;
}

void QueueMaster::updateNodeInfo()
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
//                 _parent.send( (*i), CMD_QUEUE_VICTIM_DATA ) << nCds << msg.str();
            }
        }
    }
}

void QueueMaster::setupLoad()
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

QueueMaster::QueueMaster()
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::QueueMaster( *this ))
#pragma warning(pop)
{
}

QueueMaster::~QueueMaster()
{
    clear();
    delete _impl;
}

void QueueMaster::attach( const uint128_t& id, const uint32_t instanceID )
{
    Object::attach( id, instanceID );

    LocalNodePtr localNode = getLocalNode();
    CommandQueue* queue = localNode->getCommandThreadQueue();
    registerCommand( CMD_QUEUE_GET_ITEM,
                     CommandFunc< detail::QueueMaster >(
                         _impl, &detail::QueueMaster::cmdGetItem ), queue );
}

void QueueMaster::clear()
{
    if (_impl->_distributor)
    {
        _impl->_distributor->clear();
    }
    _impl->queue.clear();
}

void QueueMaster::getInstanceData( co::DataOStream& os )
{
    os << getInstanceID() << getLocalNode()->getNodeID();
}

QueueItem QueueMaster::push()
{
    return QueueItem( *this );
}

void QueueMaster::setPackageDistributor( PackageDistributorPtr distributor )
{
    Producer::setPackageDistributor( distributor );
    _impl->_distributor = distributor;
}

PackageDistributorPtr QueueMaster::getPackageDistributor()
{
    return _impl->_distributor;
}

void QueueMaster::_addItem( QueueItem& item )
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

void QueueMaster::notifyEnd()
{
    if (_impl->_distributor)
    {
        _impl->_distributor->notifyQueueEnd();
    }
}

void QueueMaster::setSlaveNodes( const co::Nodes& nodes )
{
    if (_impl->_distributor)
    {
        _impl->_distributor->setSlaveNodes( nodes );
    }
}

void QueueMaster::setPerfLogger( co::PerfLogger *perfLogger )
{
    if (_impl->_distributor)
    {
        _impl->_distributor->setPerfLogger( perfLogger );
    }
}

} // co
