
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

#define LB_MT_FLOAT_MAP

#include "centLoadAwareDistributor.h"
#include "stealingQueueCommand.h"

#include <co/objectICommand.h>
#include <co/queueCommand.h>
#include <co/objectOCommand.h>
#include <co/queueItem.h>
#include <co/producer.h>
#include <co/perfLogger.h>
 
#include <lunchbox/array.h>
#include <lunchbox/buffer.h>
#include <lunchbox/floatMap.h>
#include <lunchbox/mtQueue.h>

#include <boost/shared_ptr.hpp>
#include <boost/concept_check.hpp>

#include <sstream>
#include <deque>

#define MAX_DISTANCE            1.f
#define SCORE_WINDOW_SIZE       128

namespace co
{

namespace detail
{

class ItemBuffer : public lunchbox::Bufferb, public lunchbox::Referenced
{
public:
    ItemBuffer( size_t number, lunchbox::Bufferb& from )
        : lunchbox::Bufferb( from )
        , lunchbox::Referenced()
        , _number( number )
    {}

    ~ItemBuffer()
    {}
    
    size_t _number;
};

typedef lunchbox::RefPtr< ItemBuffer > ItemBufferPtr;
typedef std::vector< detail::ItemBufferPtr > Items;

struct NodeInfo
{
    float       position;
    float       distLeft;
    float       distRight;
    float       totalScore;
    float       totalLoad;
};

struct Score
{
    Score( float _score, const NodeID& _nodeID )
        : score( _score )
        , nodeID( _nodeID )
    {}

    float score;
    NodeID nodeID;
};

class CentLoadAwareDistributor
{
public:
    typedef lunchbox::FloatMap< float, ItemBufferPtr > ItemMap;
    typedef std::map< NodeID, NodeInfo > NodeMap;
    typedef std::deque< Score > Scores;

    CentLoadAwareDistributor( co::CentLoadAwareDistributor& parent, Producer& producer )
        : _parent( parent )
        , _producer( producer )
        , _perfLogger( NULL )
        , _nNodes( 0 )
        , _totalScore( 0 )
    {}

    void initNodes();

    void updateNodes();

    void updateScores( float score, const NodeID& nodeID );

    Nodes _nodes;
    NodeMap _nodeMap;
    ItemMap _itemMap;
    Scores _scores;
    co::CentLoadAwareDistributor& _parent;
    co::Producer& _producer;
    co::PerfLogger* _perfLogger;
    size_t _nNodes;
    float _totalScore;
    float _totalLoad;
};

void CentLoadAwareDistributor::initNodes()
{
    LBASSERT( _nNodes > 0 );

    float maxDist = MAX_DISTANCE / _nNodes;
    _itemMap.setMaxDistance( maxDist );
    _itemMap.setMaxKey( 1.f );

    std::stringstream ss;
//     ss << "\tpos\t";
    for( size_t i = 0; i < _nNodes; ++i )
    {
        float pos = (i + .5f) / static_cast<float>( _nNodes );
        NodeInfo& nodeInfo = _nodeMap[ _nodes[ i ]->getNodeID() ];
        nodeInfo.distLeft = maxDist;
        nodeInfo.distRight = maxDist;
        nodeInfo.totalScore = 0;
        nodeInfo.totalLoad = 0;
        nodeInfo.position = pos;
        ss << pos << " ";
    }
    ss << "\t\t\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "init nodes", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::updateNodes()
{
    std::stringstream ss;
//     ss << "\tpos\t";
    if( _totalScore <= std::numeric_limits< float >::epsilon( ))
        return;

    for( size_t i = 0; i < _nNodes; ++i )
    {
        NodeID id = _nodes[ i ]->getNodeID();
        NodeInfo& nodeInfo = _nodeMap[ id ];
        float pos = nodeInfo.position;

        int prev;
        int prevIndex = prev = i - 1;
        bool left = false;
        if( prevIndex < 0 )
        {
            prevIndex += _nNodes;
            left = true;
        }
        NodeInfo prevInfo = { 0.f, 0, 0, 0, 0 };
        NodeID prevID = _nodes[ prevIndex ]->getNodeID();
        prevInfo = _nodeMap[ prevID ];
        float prevPos = left ? prevInfo.position - 1.f : prevInfo.position;

        float prevScore = prevInfo.totalScore + 1.f; /* > std::numeric_limits< float >::epsilon() ? 
            prevInfo.totalScore : 1.f;*/
        float nodeScore = nodeInfo.totalScore + 1.f; /* > std::numeric_limits< float >::epsilon() ? 
            nodeInfo.totalScore : 1.f;*/
        float sum = prevScore + nodeScore;
        float prevWeight = prevScore / sum;
        float weight = nodeScore / sum;
        float prevBorder = prevPos * prevWeight + pos * weight;

        int next;
        int nextIndex = next = i + 1;
        bool right = false;
        if( nextIndex >= static_cast<int>( _nNodes ))
        {
            nextIndex -= _nNodes;
            right = true;
        }
        NodeInfo nextInfo = { 1.f, 0, 0, 0, 0 };
        NodeID nextID = _nodes[ nextIndex ]->getNodeID();
        nextInfo = _nodeMap[ nextID ];
        float nextPos = right ? nextInfo.position + 1.f : nextInfo.position;

        float nextScore = nextInfo.totalScore + 1.f; /* > std::numeric_limits< float >::epsilon() ? 
            nextInfo.totalScore : 1.f;*/
        sum = nodeScore + nextScore;
        float nextWeight = nextScore / sum;
        weight = nodeScore / sum;
        float nextBorder = pos * weight + nextPos * nextWeight;

        float newPos = (prevBorder + nextBorder) * .5f;         // resposition
        if( newPos < 0.f )
            newPos += 1.f;
        else if( newPos > 1.f )
            newPos -= 1.f;
        nodeInfo.position = newPos;
        ss << newPos << " ";
    }
    ss << "\tdist\t";
    for( size_t i = 0; i < _nNodes; ++i )
    {
        NodeID id = _nodes[ i ]->getNodeID();
        NodeInfo& nodeInfo = _nodeMap[ id ];
        float pos = nodeInfo.position;

        int prevIndex = i - 1;
        bool left = false;
        if( prevIndex < 0 )
        {
            prevIndex += _nNodes;
            left = true;
        }
        NodeInfo prevInfo = { 0.f, 0, 0, 0, 0 };
        NodeID prevID = _nodes[ prevIndex ]->getNodeID();
        prevInfo = _nodeMap[ prevID ];
        float prevPos = left ? prevInfo.position - 1.f : prevInfo.position;

        int nextIndex = i + 1;
        bool right = false;
        if( nextIndex >= static_cast<int>( _nNodes ))
        {
            nextIndex -= _nNodes;
            right = true;
        }
        NodeInfo nextInfo = { 1.f, 0, 0, 0, 0 };
        NodeID nextID = _nodes[ nextIndex ]->getNodeID();
        nextInfo = _nodeMap[ nextID ];
        float nextPos = right ? nextInfo.position + 1.f : nextInfo.position;

        nodeInfo.distLeft = pos - prevPos;
        nodeInfo.distRight = nextPos - pos;
        ss << "(" << nodeInfo.distLeft << " " << nodeInfo.distRight << ")";
    }
    ss << "\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "updated nodes", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::updateScores(float score, const NodeID& nodeID)
{
    _scores.push_back( Score( score, nodeID ));
    if( _scores.size() > SCORE_WINDOW_SIZE )
        _scores.pop_front();

    float sum = 0.f;
    _totalScore = 0;
    for( Scores::const_iterator it = _scores.begin();
         it != _scores.end();
         ++it )
    {
        sum += it->score;
        _nodeMap[ it->nodeID ].totalScore = 0;
    }

    for( Scores::const_iterator it = _scores.begin();
         it != _scores.end();
         ++it )
    {
        NodeInfo &nodeInfo = _nodeMap[ it->nodeID ];
        nodeInfo.totalScore += it->score;
    }
    _totalScore += sum;

    _nodeMap[ nodeID ].totalLoad += score;
    _totalLoad += score;

    std::stringstream ss;
    for( size_t i = 0; i < _nNodes; ++i )
    {
        NodeID id = _nodes[ i ]->getNodeID();
        ss << _nodeMap[ id ].totalScore << " ";
    }
    ss << "\ttotal\t" << _totalScore << "\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "updated scores", ss.str(), "SERVER");
}

}

CentLoadAwareDistributor::CentLoadAwareDistributor( Producer& producer )
#pragma warning(push)
#pragma warning(disable: 4355)
    : _impl( new detail::CentLoadAwareDistributor( *this, producer ) )
#pragma warning(pop)
{
}

CentLoadAwareDistributor::~CentLoadAwareDistributor()
{
    delete _impl;
}

void CentLoadAwareDistributor::pushItem( QueueItem &item )
{
    float pos = item.getPositionHint();
    std::stringstream ss;
    size_t number = item.getNumber();
    ss << pos << "\tnumber\t" << number << "\t\t\t";
    _impl->_perfLogger->log(_impl->_producer.getLocalNode()->getNodeID(), "pushing item at", ss.str(), "SERVER");

    detail::ItemBufferPtr newBuffer = new detail::ItemBuffer( number, item.getBuffer( ));
    _impl->_itemMap.insert( pos, newBuffer );
}

bool CentLoadAwareDistributor::cmdGetItem( co::ICommand& comd )
{
    size_t nNodes = _impl->_nodeMap.size();
    if( nNodes < 1 )
    {
        _impl->initNodes();
    }
    else
    {
        _impl->updateNodes();
    }
    LBASSERT( _impl->_nodeMap.size() > 0 );

    NodeID nodeID = comd.getNode()->getNodeID();
    co::ObjectICommand command( comd );
    const uint32_t itemsRequested = command.get< uint32_t >();
    const float score LB_UNUSED = command.get< float >() + .1f;
    const uint32_t slaveInstanceID = command.get< uint32_t >();
    const int32_t requestID = command.get< int32_t >();

//     std::cout << "score: " << score << std::endl;

    detail::NodeInfo& nodeInfo = _impl->_nodeMap[ nodeID ];
//     _impl->_itemMap.setMaxDistance( maxDist );

    detail::Items items;
    if( nodeInfo.distLeft > 0.f && nodeInfo.distRight > 0.f )
    {
        _impl->_itemMap.setMaxDistance( nodeInfo.distLeft, nodeInfo.distRight );
    }
    _impl->_itemMap.tryRemove( itemsRequested, nodeInfo.position, items );

    std::stringstream ss;
    ss << nodeInfo.position << "\tnumbers\t( ";
    float totalScore = 0;
    for( detail::Items::const_iterator i = items.begin(); i != items.end(); ++i )
    {
        const detail::ItemBufferPtr item = *i;
        if( !item->isEmpty( ))
        {
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID )
                << Array< const void >( item->getData(), item->getSize( ));
            ss << item->_number << " ";
            totalScore += 1.f;
        }
    }
    ss << ")\trequested\t" << itemsRequested << "\t";
    _impl->_perfLogger->log( nodeID, "popped items from", ss.str( ), "SERVER");

    _impl->updateScores( totalScore, nodeID );

    if( itemsRequested > items.size( ))
    {
        if( _impl->_itemMap.size() < 1 )
        {
            _impl->_producer.send( command.getNode(), CMD_QUEUE_EMPTY, slaveInstanceID ) << requestID;
        }
        else
        {
            // dummy message to prevent slave from being stuck
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID ) << requestID;
        }
    }

    return true;
}

void CentLoadAwareDistributor::notifyQueueEnd()
{
    // TEST
//     _impl->_totalScore = 0;
//     _impl->initNodes();

    std::stringstream ss;
    _impl->_totalLoad = 0;
    for( size_t i = 0; i < _impl->_nNodes; ++i )
    {
        NodeID id = _impl->_nodes[ i ]->getNodeID();
        detail::NodeInfo &nodeInfo = _impl->_nodeMap[ id ];
        ss << nodeInfo.totalLoad << " ";
        nodeInfo.totalLoad = 0;
    }
    ss << "\ttotal\t" << _impl->_totalLoad << "\t\t\t";
    _impl->_perfLogger->log( _impl->_producer.getLocalNode()->getNodeID(), "finished queue with load", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::setSlaveNodes(const co::Nodes& nodes)
{
    _impl->_nodes = nodes;
    _impl->_nNodes = nodes.size();
}

void CentLoadAwareDistributor::setPerfLogger(co::PerfLogger* perfLogger)
{
    _impl->_perfLogger = perfLogger;
}

} // co
