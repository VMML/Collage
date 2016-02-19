
/* Copyright (c) 2014-2016, David Steiner      <steiner@ifi.uzh.ch>
 *               2016,      Enrique G. Paredes <egparedes@ifi.uzh.ch>
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

#include <fstream>
#include <sstream>
#include <deque>
#include <cmath>

#define MAX_DISTANCE            1.f
#define SCORE_WINDOW_SIZE       128
#define LOG_NODE_POS

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

struct NodeInfo
{
    float       position;
    float       distLeft;
    float       distRight;
    float       totalScore;
    float       totalLoad;
    int         idleCounter;
};

std::ostream& operator << (std::ostream& os, const NodeInfo& ni)
{
    return os << "position: " << ni.position << ", distLeft: " << ni.distLeft << ", distRight: " << ni.distRight << ", totalScore: " << ni.totalScore << ", totalLoad: " << ni.totalLoad;
}

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
    {
#ifdef LOG_NODE_POS
        _outFile.open( "centLoadAwareDistributor.log" );
#endif
    }

    void initNodes();

    void updateNodes();

    void updateScores( float score, const NodeID& nodeID );

#ifdef LOG_NODE_POS
    std::ofstream _outFile;
#endif
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
    _itemMap.setKeyInterval( 0.f, 1.f );

    std::stringstream ss;
    ss << "\tpos\t";
    for( size_t i = 0; i < _nNodes; ++i )
    {
        float pos = (i + .5f) / static_cast<float>( _nNodes );
        NodeInfo& nodeInfo = _nodeMap[ _nodes[ i ]->getNodeID() ];
        nodeInfo.distLeft = maxDist;
        nodeInfo.distRight = maxDist;
        nodeInfo.totalScore = 0;
        nodeInfo.totalLoad = 0;
        nodeInfo.idleCounter = 0;
        nodeInfo.position = pos;
        ss << pos << " ";
#ifdef LOG_NODE_POS
        _outFile << "init_node: " << i << ", " << nodeInfo << std::endl;
#endif
    }
#ifdef LOG_NODE_POS
    _outFile.flush();
#endif
    ss << "\t\t\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "init nodes", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::updateNodes()
{
    std::stringstream ss;
    if( _totalScore <= std::numeric_limits< float >::epsilon( ))
        return;

    ss << "\tpos\t";
    std::vector< float > newPositions(_nNodes );
    for( size_t i = 0; i < _nNodes; ++i )
    {
        const NodeInfo& nodeInfo = _nodeMap[ _nodes[ i ]->getNodeID() ];
        const float pos = _nodeMap[ _nodes[ i ]->getNodeID() ].position;

        int prevIndex = i - 1;
        if( prevIndex < 0 )
            prevIndex = _nNodes - 1;

        const NodeInfo& prevInfo = _nodeMap[ _nodes[ prevIndex ]->getNodeID() ];
        const float prevPos = ( prevInfo.position > pos ) ? prevInfo.position - 1.f : prevInfo.position;

        const float prevScore = prevInfo.totalScore + 1.f;
        const float nodeScore = nodeInfo.totalScore + 1.f;
        float addedScore = prevScore + nodeScore;
        const float prevWeight = prevScore / addedScore;
        float weight = nodeScore / addedScore;
        const float prevBorder = prevPos * weight + pos * prevWeight;

        int nextIndex = i + 1;
        if( nextIndex >= static_cast<int>( _nNodes ))
            nextIndex = 0;

        const NodeInfo& nextInfo = _nodeMap[ _nodes[ nextIndex ]->getNodeID() ];
        const float nextPos = ( nextInfo.position < pos ) ? nextInfo.position + 1.f : nextInfo.position;

        const float nextScore = nextInfo.totalScore + 1.f;
        addedScore = nodeScore + nextScore;
        const float nextWeight = nextScore / addedScore;
        weight = nodeScore / addedScore;
        const float nextBorder = pos * nextWeight + nextPos * weight;

        float newPos = (prevBorder + nextBorder) * .5f;         // node repositioning
        if( newPos < 0.f )
            newPos += 1.f;
        else if( newPos > 1.f )
            newPos -= 1.f;
        newPositions[i]= newPos;
        ss << newPos << " ";
    }
    ss << "\tdist\t";
    for( size_t i = 0; i < _nNodes; ++i )
    {
        const float pos = newPositions[i];
        NodeInfo& nodeInfo = _nodeMap[ _nodes[ i ]->getNodeID() ];

        nodeInfo.position = pos;

        int prevIndex = i - 1;
        if( prevIndex < 0 )
            prevIndex = _nNodes - 1;

        float prevPos = newPositions[prevIndex];
        if( prevPos > pos )
            prevPos -= 1.f;

        nodeInfo.distLeft = pos - prevPos;

        int nextIndex = i + 1;
        if( nextIndex >= static_cast<int>( _nNodes ))
            nextIndex = 0;

        float nextPos = newPositions[nextIndex];
        if( nextPos < pos )
            nextPos += 1.f;

        nodeInfo.distRight = nextPos - pos;

#ifdef LOG_NODE_POS
        _outFile << "node: " << i << ", " << nodeInfo << std::endl;
#endif
        ss << "(" << nodeInfo.distLeft << " " << nodeInfo.distRight << ")";
    }
    ss << "\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "updated nodes", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::updateScores(float score, const NodeID& nodeID)
{
    _scores.push_back( Score( score, nodeID ));
    _nodeMap[ _scores.back().nodeID ].totalScore += score;

    if( _scores.size() > SCORE_WINDOW_SIZE )
    {
        _nodeMap[ _scores.front().nodeID ].totalScore -= _scores.front().score;
        _scores.pop_front();
    }

    float sum = 0.f;
    for( Scores::const_iterator it = _scores.begin();
         it != _scores.end();
         ++it )
    {
        sum += it->score;
    }
    _totalScore = sum;

    _nodeMap[ nodeID ].totalLoad += score;
    _totalLoad += score;

    std::stringstream ss;
    for( size_t i = 0; i < _nNodes; ++i )
    {
        NodeID id = _nodes[ i ]->getNodeID();
        ss << _nodeMap[ id ].totalScore << " ";
    }
    ss << "\ttotal\t" << _totalScore << "\t\t\t";
    _perfLogger->log( _producer.getLocalNode()->getNodeID(), "updated scores", ss.str(), "SERVER" );
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

    detail::ItemBufferPtr newBuffer = new detail::ItemBuffer( item.getBuffer( ));
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

    detail::NodeInfo& nodeInfo = _impl->_nodeMap[ nodeID ];

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
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID ) << false
                << Array< const void >( item->getData(), item->getSize( ));
            totalScore += 1.f;
            nodeInfo.idleCounter = 0;
        }
    }
    ss << ")\trequested\t" << itemsRequested << "\t";

    if( totalScore > .0f )
    {
        _impl->updateScores( totalScore, nodeID );
        _impl->_perfLogger->log( nodeID, "popped items from", ss.str( ), "SERVER");
    }
    if( itemsRequested > items.size( ))
    {
        if( _impl->_itemMap.size() < 1 )
        {
            _impl->_producer.send( command.getNode(), CMD_QUEUE_EMPTY, slaveInstanceID ) << requestID;
            _impl->_perfLogger->log( nodeID, "sent empty queue command", ss.str( ), "SERVER");
            nodeInfo.idleCounter = 0;
        }
        else
        {
            // dummy message to prevent slave from being stuck
            _impl->_producer.send( command.getNode(), CMD_QUEUE_ITEM, slaveInstanceID ) << true;
            _impl->_perfLogger->log( nodeID, "sent wait command", ss.str( ), "SERVER");
            if( nodeInfo.idleCounter > 2000 )
            {
                exit(-1);
            }
            ++nodeInfo.idleCounter;
        }
    }

    return true;
}

void CentLoadAwareDistributor::notifyQueueEnd()
{
    std::stringstream ss;
    for( size_t i = 0; i < _impl->_nNodes; ++i )
    {
        NodeID id = _impl->_nodes[ i ]->getNodeID();
        detail::NodeInfo &nodeInfo = _impl->_nodeMap[ id ];
        ss << nodeInfo.totalLoad << " ";
        nodeInfo.totalLoad = 0;
    }
    ss << "\ttotal\t" << _impl->_totalLoad << "\t\t\t";
    _impl->_totalLoad = 0;

    _impl->_perfLogger->log( _impl->_producer.getLocalNode()->getNodeID(), "finished queue with load", ss.str(), "SERVER");
}

void CentLoadAwareDistributor::setSlaveNodes(const co::Nodes& nodes)
{
    _impl->_nodes = nodes;
    _impl->_nNodes = nodes.size();
    if( _impl->_nodeMap.size() < 1 )
    {
        _impl->initNodes();
    }
}

void CentLoadAwareDistributor::setPerfLogger(co::PerfLogger* perfLogger)
{
    _impl->_perfLogger = perfLogger;
}

} // co
