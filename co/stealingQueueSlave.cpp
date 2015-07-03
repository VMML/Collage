
/* Copyright (c) 2011-2013, Stefan Eilemann <eile@eyescale.ch>
 *                    2011, Carsten Rohn <carsten.rohn@rtt.ag>
 *               2011-2012, Daniel Nachbaur <danielnachbaur@gmail.com>
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

#include "stealingQueueSlave.h"

#include "buffer.h"
#include "commandQueue.h"
#include "dataIStream.h"
#include "global.h"
#include "objectOCommand.h"
#include "objectICommand.h"
#include "stealingQueueCommand.h"
#include "nodeCommand.h"
#include "exception.h"
#include "connectionDescription.h"

#include <lunchbox/scopedMutex.h>
#include <lunchbox/condition.h>
#include <lunchbox/mtQueue.h>
#include <lunchbox/thread.h>
#include <lunchbox/clock.h>
#include <lunchbox/sleep.h>
#include <lunchbox/lock.h>
// #include <eq/fabric/tile.h>     // TEST

#include <boost/crc.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <iterator>
#include <deque>
#include <cstdlib>
#include <ctime>

// using namespace eq::fabric; // TEST

// TEST
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

static int calcCrc32(void const *buffer, std::size_t byteCount)   // TEST
{
    boost::crc_32_type result;
    result.process_bytes(buffer, byteCount);
    return result.checksum();
}

static void printRawData(void const *buffer, std::size_t byteCount)
{
    const unsigned char* tmp = static_cast< const unsigned char* >(buffer);
    
    std::cerr << "Raw data: ";
    for (size_t i = 0; i < byteCount; ++i)
    {
        std::cerr << std::hex << static_cast< uint >(tmp[i]) << " ";
    }
    std::cerr << std::endl;
}

namespace co
{
namespace detail
{

struct Victim
{
    NodePtr node;
    int64_t timeout;

    Victim()
        : timeout(0)
    {}

    Victim(NodePtr node_, uint64_t timeout_)
        : node(node_)
        , timeout(timeout_)
    {}

    explicit Victim(NodePtr node_)
        : node(node_)
        , timeout(0)
    {}

    Victim(const Victim &other)
        : node(other.node)
        , timeout(other.timeout)
    {}

    Victim &operator =(const Victim &other)
    {
        node = other.node;
        timeout = other.timeout;
        return *this;
    }

    bool operator ==(const Victim &other)
    {
        if (other.node->getNodeID() == node->getNodeID())
        {
            return true;
        }
        return false;
    }

    bool isValid()
    {
        return node.isValid();
    }
};

class Thief : public lunchbox::Thread
{
public:

    typedef std::deque< Victim > Victims;

    Thief(co::detail::StealingQueueSlave& parent)
        : lunchbox::Thread()
        , running_(true)
        , cFinish_(0)
        , lFinish_(0)
        , nNodes_(0)
        , cVictim_(0)
        , finished_(true)
        , parent_( parent )
    {
        std::cout << "Thief awakes . . ." << std::endl;
    }
    
    virtual ~Thief() {}
    Thief& operator=( const Thief& ) { return *this; }

    void setupVictims();
    void relegateVictim(const Victim &victim);
    void eraseVictim(const Victim &victim);
    void finish();
    virtual void run();

    bool running_;
    int64_t cFinish_;
    int64_t lFinish_;
    size_t nNodes_;
    size_t cVictim_;
    Victims victims_;
    lunchbox::Condition condFinish_;
    lunchbox::Condition condHalt_;
    lunchbox::Clock clock_;
    lunchbox::Lock lock_;
    co::CommandQueue cmdQueue_;
    bool finished_;

    co::detail::StealingQueueSlave& parent_;
};

class StealingQueueSlave : public co::Dispatcher
{
public:
    
    static const int STEAL_TIMEOUT = 33;
    
    StealingQueueSlave(co::StealingQueueSlave& parent, const uint32_t mark, const uint32_t amount);
    
    bool cmdQueueVictimData( co::ICommand& comd );
    bool cmdQueueHello( co::ICommand& comd );       // TEST
    bool cmdQueueItemLeft( co::ICommand& comd );
    bool cmdQueueItemRight( co::ICommand& comd );
    bool cmdQueueEmpty( co::ICommand& comd );
    bool cmdMasterQueueEmpty( co::ICommand& comd );
    bool cmdQueueDeny( co::ICommand& comd );
    bool cmdQueueDenyMaster( co::ICommand& comd );
    bool cmdStolenItem( co::ICommand& comd );
    bool cmdStealItem( co::ICommand& comd );

    typedef lunchbox::MTQueue< ConstBufferPtr > ItemQueue;

    ItemQueue queueLeft_;
    ItemQueue queueRight_;
    NodePtr master_;
    uint32_t masterInstanceID_;

    const uint32_t prefetchMark_;
    const uint32_t prefetchAmount_;
    
    lunchbox::Clock clock_;
    bool masterFinished_;
//     lunchbox::Lockable< bool > masterFinished_;

    Thief thief_;
    bool victimInfoSent_;
    lunchbox::Condition itemsAvailable_;
//     lunchbox::Lockable< int > nItems_;
    co::StealingQueueSlave& parent_;
};

StealingQueueSlave::StealingQueueSlave(co::StealingQueueSlave& parent, const uint32_t mark, const uint32_t amount)
    : co::Dispatcher()
    , masterInstanceID_( CO_INSTANCE_ALL )
    , prefetchMark_( mark == LB_UNDEFINED_UINT32 ?
                    10 :                         // TEST
                    mark )
    , prefetchAmount_( amount == LB_UNDEFINED_UINT32 ?
                      Global::getIAttribute( Global::IATTR_QUEUE_REFILL ) :
                      amount )
    , masterFinished_( false )
    , thief_(*this)
    , victimInfoSent_(false)
//     , nItems_(0)
    , parent_( parent )
{
    std::cerr << "local node: " << parent_.getLocalNode() << std::endl;
}

bool StealingQueueSlave::cmdQueueItemLeft( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     lunchbox::ScopedMutex<> mutex(nItems_);
    
//     std::cout << "**************** StealingQueueSlave::cmdQueueItemLeft" << std::endl;
    queueLeft_.push(command.getBuffer());
    std::cout << clock_.getTime64() << " Item queued at front queue (size: " << queueLeft_.getSize() << ") . . ." << std::endl;

//     ++(*nItems_);
//     std::cout << clock_.getTime64() << "StealingQueueSlave::cmdQueueItemLeft, items: " << *nItems_ << " . . ." << std::endl;

    return true;
}

bool StealingQueueSlave::cmdQueueItemRight( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     lunchbox::ScopedMutex<> mutex(nItems_);
    
//     std::cout << "**************** StealingQueueSlave::cmdQueueItemRight" << std::endl;
    queueRight_.push(command.getBuffer());
//     std::cout << clock_.getTime64() << " Item queued at back queue (size: " << queueRight_.getSize() << ") . . ." << std::endl;

//     ++(*nItems_);
//     std::cout << clock_.getTime64() << "StealingQueueSlave::cmdQueueItemRight, items: " << *nItems_ << " . . ." << std::endl;

    return true;
}

bool StealingQueueSlave::cmdQueueEmpty( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     std::cout << "**************** StealingQueueSlave::cmdQueueEmpty" << std::endl;
    thief_.cmdQueue_.push(command);

    return true;
}

bool StealingQueueSlave::cmdMasterQueueEmpty( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     lunchbox::ScopedMutex<> finishedMutex(masterFinished_);
    
//     std::cout << "**************** StealingQueueSlave::cmdMasterQueueEmpty" << std::endl;
    masterFinished_ = true;

    return true;
}

bool StealingQueueSlave::cmdQueueDeny( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     std::cout << "**************** StealingQueueSlave::cmdQueueDeny" << std::endl;
    thief_.cmdQueue_.push(command);

    return true;
}

bool StealingQueueSlave::cmdQueueDenyMaster( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     std::cout << "**************** StealingQueueSlave::cmdQueueDenyMaster" << std::endl;
    thief_.cmdQueue_.push(command);

    return true;
}

bool StealingQueueSlave::cmdStolenItem( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
//     std::cout << "**************** StealingQueueSlave::cmdStolenItem" << std::endl;
    thief_.cmdQueue_.push(command);

    return true;
}

// TEST
bool StealingQueueSlave::cmdQueueHello( co::ICommand& comd )
{
    co::ObjectICommand command( comd );

//     std::cout << "**************** StealingQueueSlave::cmdQueueHello" << std::endl;

    NodePtr node = command.getNode();
    NodeID nodeID;
    command >> nodeID;
    std::cout << parent_.getLocalNode()->getNodeID() << ": Node " << node->getNodeID() << " says hello . . . " << std::endl;

    return true;
}

bool StealingQueueSlave::cmdStealItem( co::ICommand& comd )
{
    co::ObjectICommand command( comd );

    const uint32_t ratio = command.get< uint32_t >();
    const uint32_t slaveInstanceID = command.get< uint32_t >();
    const int32_t requestID = command.get< int32_t >();
    const uint32_t itemsRequested = ratio * queueLeft_.getSize() / 255;

    Connections connections( 1, command.getNode()->getConnection( ));

    if (itemsRequested < 1)
    {
        std::cerr << "Nothing to hand over (size: " << queueLeft_.getSize() << ") . . ." << std::endl;
        co::ObjectOCommand( connections, CMD_QUEUE_DENY,
                            COMMANDTYPE_OBJECT, command.getObjectID(),
                            slaveInstanceID ) << requestID;
        return true;
    }
    
    NodePtr node = command.getNode();
    LocalNodePtr localNode = parent_.getLocalNode();
    std::cerr << localNode->getNodeID() << ": trying to hand over " << itemsRequested << " tasks (size: " << queueLeft_.getSize() << ") to " << node->getNodeID() << ". . ." << std::endl;
    
    thief_.relegateVictim(Victim(node));

    typedef std::vector< ConstBufferPtr > Items;
    Items items;
    queueLeft_.tryPop( itemsRequested, items );

    uint32_t itemsDelivered = 0;
    for( Items::iterator i = items.begin(); i != items.end(); ++i )
    {
        co::ObjectICommand item(localNode, localNode, *i, false);
        if (item.isValid())
        {
//             Tile tile;
//             item >> tile;
//             uint32_t checksum = calcCrc32(&tile.frustum, sizeof(tile) - sizeof(tile.checksum) - sizeof(tile.stolen));
//             std::cerr << "Popped tile for handover " << std::hex << checksum << std::dec
//                       << ((checksum == tile.checksum) ? " (checksum OK): " : " (checksum NOT OK): ")
//                       << "[ " << tile.frustum << " " << tile.ortho << " " << tile.pvp << " " << tile.vp << " ]" << std::endl; // TEST
//             printRawData(&tile, sizeof(tile));

            co::ObjectOCommand cmd( connections, CMD_QUEUE_STOLEN_ITEM,
                                    COMMANDTYPE_OBJECT, parent_.getID(),
                                    slaveInstanceID );

            size_t size = item.getSize();   // FIXME: why is this much bigger than the actual data sent?
//                     std::cerr << "Sending " << size << " . . ." << std::endl;
            std::vector<uint8_t> msg(size);
            Array< uint8_t > arr(&msg[0], msg.size());
            item >> arr;
//             std::cout << "MSG: ";
//             for (size_t j = 0; j < arr.getNumBytes(); ++j)
//             {
//                 std::cout << std::hex << static_cast< uint >(arr.data[j]) << " ";
//             }
//             std::cout << std::endl;
            cmd << arr;

//             tile.stolen = true;
//             cmd << tile;
            ++itemsDelivered;
        }
    }

    if (itemsDelivered > 0)
    {
        std::cerr << "Finished queue . . ." << std::endl;
        co::ObjectOCommand cmd( connections, CMD_QUEUE_EMPTY,
                                COMMANDTYPE_OBJECT, parent_.getID(),
                                slaveInstanceID );
        cmd << itemsDelivered;
    }

    return true;
}

// HACK
bool StealingQueueSlave::cmdQueueVictimData( co::ICommand& comd )
{
    co::ObjectICommand command( comd );
    
//     std::cout << "**************** StealingQueueSlave::cmdQueueVictimData" << std::endl;

    LocalNodePtr localNode = parent_.getLocalNode();

    int nCds = 0;
    command >> nCds;
    std::string msg;
    command >> msg;
    std::stringstream ss(msg);

    const ConnectionDescriptions &localCds = localNode->getConnectionDescriptions();    // TEST
    for (int i = 0; i < nCds; ++i)
    {
        std::string data;
        ss >> data;
        ConnectionDescriptions cds;
        deserialize(data, cds);

        for( ConnectionDescriptionsCIter j = cds.begin();
                j != cds.end(); ++j )
        {
            bool noIntersect = true;
            const ConnectionDescriptionPtr &cd = *j;
            for (ConnectionDescriptionsCIter k = localCds.begin(); k != localCds.end(); ++k)    // HACK
                if ((*k)->type      == cd->type         &&
                        (*k)->port      == cd->port         &&
                        (*k)->hostname  == cd->hostname)
                {
                    noIntersect = false;
                    break;
                }

            if (noIntersect)
            {
                std::cout << "Conn Desc: " << cd << std::endl;

                co::NodePtr nodeProxy = new co::Node;
                nodeProxy->addConnectionDescription( cd );
                localNode->connect( nodeProxy );
            }
        }
    }
    victimInfoSent_ = true;

    return true;
}

void Thief::setupVictims()
{
    Nodes nodes;
    LocalNodePtr localNode = parent_.parent_.getLocalNode();
    if (localNode)
    {
        localNode->getNodes(nodes, false);
        size_t nNodes = nodes.size();

        if (nNodes_ != nNodes)
        {
            std::cout << "Thief: setting up victims . . ." << std::endl;
            
            nNodes_ = nNodes;
            cVictim_ = 0;
            victims_.clear();

            int64_t time = clock_.getTime64();

            for (Nodes::const_iterator i = nodes.begin(); i != nodes.end(); ++i)
                if ((*i)                                                &&
                    (*i)->getNodeID() != parent_.master_->getNodeID()    &&
                    (*i)->getNodeID() != localNode->getNodeID())
                {
                    std::cout << ". . . " << (*i)->getNodeID() << std::endl;
                    victims_.push_back(Victim(*i, time));
                }
            std::random_shuffle(victims_.begin(), victims_.end());
        }
    }
}

void Thief::relegateVictim(const Victim &victim)
{
    Victims::iterator finding = std::find(victims_.begin(), victims_.end(), victim);
    if (finding != victims_.end())
    {
        Victim tmp(*finding);
        victims_.erase(finding);
        tmp.timeout = clock_.getTime64() + 100;  // TEST
        victims_.push_front(tmp);
        std::cerr << "Thief: relegated victim " << victim.node->getNodeID()
            << ", timeout: " << tmp.timeout
            << "." << std::endl;
    }
    else
    {
        std::cerr << "Thief ERROR: couldn't relegate victim " << victim.node->getNodeID() << "." << std::endl;
    }
}

void Thief::eraseVictim(const Victim &victim)
{
    Victims::iterator finding = std::find(victims_.begin(), victims_.end(), victim);
    if (finding != victims_.end())
    {
        victims_.erase(finding);
        std::cerr << "Thief: erased victim " << victim.node->getNodeID() << ", remaining victims: " << victims_.size() << "." << std::endl;
    }
    else
    {
        std::cerr << "Thief ERROR: couldn't erase victim " << victim.node->getNodeID() << "." << std::endl;
    }
}

void Thief::finish()
{
    std::cout << "Thief: finishing . . ." << std::endl;
    cFinish_ = clock_.getTime64();
    std::cout << "Thief: finishing time " << ( cFinish_ - lFinish_ ) << std::endl;
    lFinish_ = cFinish_;
}

void Thief::run()
{
    static lunchbox::a_int32_t _request;
    const int32_t request = ++_request;

    while( true )   // TEST
    {
        setupVictims();
        lunchbox::ScopedMutex<> thiefMutex(lock_);

        const size_t queueSize = parent_.queueLeft_.getSize();
        bool receive = false;
        if( queueSize <= parent_.prefetchMark_ )
        {
            if (victims_.size() > cVictim_)
            {
                const Victim &victim = victims_[cVictim_];
                cVictim_ = (cVictim_ + 1) % victims_.size();
                if (clock_.getTime64() >= victim.timeout)
                {
                    NodePtr node = victim.node;
                    std::cout << "Thief: attempting to steal tasks from node " << node->getNodeID() << " . . ." << std::endl;

                    parent_.parent_.send( node, CMD_QUEUE_STEAL_ITEM )
                            << uint32_t(128) << parent_.parent_.getInstanceID() << request;     // TEST
                    receive = true;
                }
            }
        }

        uint32_t nStolen = 0;
        uint32_t nDelivered = 0;
        while (receive) try
        {
            co::ObjectICommand cmd(cmdQueue_.pop(500));    // TEST
            switch( cmd.getCommand( ))
            {
            case CMD_QUEUE_STOLEN_ITEM:
            {
                parent_.queueLeft_.push(cmd.getBuffer());
                std::cout << clock_.getTime64() << " Thief: item stolen (queue size: " << parent_.queueLeft_.getSize() << ") . . ." << std::endl;

//                 lunchbox::ScopedMutex<> mutex(parent_.nItems_);
//                 ++(*parent_.nItems_);
//                 std::cout << clock_.getTime64() << "Thief::run, items: " << *parent_.nItems_ << " . . ." << std::endl;
                ++nStolen;
            }
                break;
                
            case CMD_QUEUE_DENY:
                if( cmd.get< int32_t >() == request )
                {
                    // TEST
                    std::cout << "Thief (" << parent_.parent_.getLocalNode()->getNodeID() << "): item denied by " << cmd.getNode()->getNodeID() << " . . ." << std::endl;
                    receive = false;
                    relegateVictim(Victim(cmd.getNode()));
                }
                break;
                
            case CMD_QUEUE_DENY_MASTER:     // thiefes should not contact master
                // TEST
                std::cout << "Thief (" << parent_.parent_.getLocalNode()->getNodeID() << "): item denied by master " << cmd.getNode()->getNodeID() << ". . ." << std::endl;
                receive = false;
                eraseVictim(Victim(cmd.getNode()));
                break;

            case CMD_QUEUE_EMPTY:
                cmd >> nDelivered;
                
                std::cout << "Thief (" << parent_.parent_.getLocalNode()->getNodeID() << "): victim queue is empty (" << nDelivered << " items delivered) . . ." << std::endl;
                receive = false;
                break;
                            
            default:
                LBUNIMPLEMENTED;
            }
        }
        catch (co::Exception& e)
        {
            LBWARN << e.what() << std::endl;
            break;
        }

        if (nStolen)
            std::cout << clock_.getTime64() << " Thief: stolen items: " << nStolen << " . . ." << std::endl;
        
        if (nStolen != nDelivered)
        {
            LBWARN << "nStolen (" << nStolen << ") != nDelivered (" << nDelivered << ")" << std::endl;
        }
    }
}
}

StealingQueueSlave::StealingQueueSlave( const uint32_t prefetchMark,
                                        const uint32_t prefetchAmount )
    : impl_( new detail::StealingQueueSlave(*this, prefetchMark, prefetchAmount ))
{
    srand(time(NULL));
}

StealingQueueSlave::~StealingQueueSlave()
{
    clear();
    delete impl_;
}

void StealingQueueSlave::attach( const uint128_t& id, const uint32_t instanceID )
{
    CommandQueue* queue = getLocalNode()->getCommandThreadQueue();
    Object::attach(id, instanceID);

//     registerCommand( CMD_QUEUE_ITEM, CommandFunc<Object>(0, 0), &impl_->cmdQueue );
//     registerCommand( CMD_QUEUE_DENY, CommandFunc<Object>(0, 0), &impl_->thief.queue);
//     registerCommand( CMD_QUEUE_DENY_MASTER, CommandFunc<Object>(0, 0), &impl_->thief.queue);
//     registerCommand( CMD_QUEUE_STOLEN_ITEM, CommandFunc<Object>(0, 0), &impl_->thief.queue );
//     registerCommand( CMD_QUEUE_STEAL_ITEM, CommandFunc<Object>(0, 0), &impl_->cmdQueue );
    
    registerCommand( CMD_QUEUE_VICTIM_DATA,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueVictimData ), queue);
    registerCommand( CMD_QUEUE_HELLO,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueHello ), queue);
    registerCommand( CMD_QUEUE_EMPTY,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueEmpty ), queue );
    registerCommand( CMD_MASTER_QUEUE_EMPTY,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdMasterQueueEmpty ), queue );
    registerCommand( CMD_QUEUE_ITEM_LEFT,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueItemLeft ), queue );
    registerCommand( CMD_QUEUE_ITEM_RIGHT,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueItemRight ), queue );
    registerCommand( CMD_QUEUE_DENY,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueDeny ), queue );
    registerCommand( CMD_QUEUE_DENY_MASTER,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdQueueDenyMaster ), queue );
    registerCommand( CMD_QUEUE_STOLEN_ITEM,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdStolenItem ), queue );
    registerCommand( CMD_QUEUE_STEAL_ITEM,
                     CommandFunc< detail::StealingQueueSlave >(
                         impl_, &detail::StealingQueueSlave::cmdStealItem ), queue );
}

void StealingQueueSlave::applyInstanceData( co::DataIStream& is )
{
    uint128_t masterNodeID;
    is >> impl_->masterInstanceID_ >> masterNodeID;

    LBASSERT( masterNodeID != 0 );
    LBASSERT( !impl_->master_ );
    LocalNodePtr localNode = getLocalNode();
    impl_->master_ = localNode->connect( masterNodeID );

    localNode->listen();
}

ObjectICommand StealingQueueSlave::pop( const uint32_t timeout )
{
    return pop(timeout, 0);
}

ObjectICommand StealingQueueSlave::pop( const uint32_t timeout, uint32_t )
{
//         lunchbox::ScopedMutex<> thiefMutex(impl_->thief_.lock_);
        try
        {
            if ((impl_->queueLeft_.getSize() + impl_->queueRight_.getSize()) < impl_->prefetchMark_)
            {
//                 std::cout << "Sending starvation message to master . . ." << std::endl;
                send( impl_->master_, CMD_QUEUE_SLAVE_FEEDBACK, impl_->masterInstanceID_ ) << true << int64_t(0) << false;   // slave is starving
            }
            
            ConstBufferPtr item;
            lunchbox::Clock clock;
            do
            {
//                 {
//                     lunchbox::ScopedMutex<> mutex(impl_->nItems_);
//                     std::cout << impl_->clock_.getTime64() << "StealingQueueSlave::pop, items: " << *impl_->nItems_ << " . . ." << std::endl;

//                     if (*impl_->nItems_)
//                     {
//                         --(*impl_->nItems_);
//                     }
//                     else
//                     {
//                         lunchbox::ScopedMutex<> finishedMutex(impl_->masterFinished_);
//                         if (!(*impl_->masterFinished_)) continue;
//                         
//                         std::cout << impl_->clock_.getTime64() << " No items left . . ." << std::endl;
//                         std::cout << impl_->clock_.getTime64() << " Queue size: " << impl_->queueLeft_.getSize() << " . . ." << std::endl;
//                         
//                         impl_->masterFinished_ = false;
//                         return ObjectICommand( 0, 0, 0, false );
//                     }
//                 }
                if( impl_->masterFinished_ )
                {
                    impl_->masterFinished_ = false;
                    return ObjectICommand( 0, 0, 0, false );
                }

//                 bool pushRight = (impl_->queueRight_.getSize() > impl_->queueLeft_.getSize()) ? true  :
//                                 (impl_->queueRight_.getSize() < impl_->queueLeft_.getSize()) ? false :
//                                 rand() % 2;
//                 if (pushRight)
//                 {
                    impl_->queueRight_.tryPop( item );
//                 }
//                 else
//                 {
//                     impl_->queueLeft_.tryPop( item );
//                 }

                if (clock.getTime64() >= timeout)   // TEST
                {
                    std::cout << impl_->clock_.getTime64() << " Timeout (queue size: " << impl_->queueLeft_.getSize() << ") . . ." << std::endl;
                    
                    return ObjectICommand( 0, 0, 0, false );
                }
            }
            while (!item);
            
            LocalNodePtr localNode = getLocalNode();
            ObjectICommand command(localNode, localNode, item, false);
            
            uint32_t flags_ = 0;
            switch( command.getCommand() )
            {
            case CMD_QUEUE_ITEM_LEFT:  // FT
//                 flags_ |= Consumer::FLAG_QUEUE_RIGHT;   // ?
            case CMD_QUEUE_ITEM_RIGHT:
            {
//                 std::cout << "Item returned (queue size: " << impl_->queueLeft_.getSize() << ") . . ." << std::endl;
//                 
//                 if (flags)
//                 {
//                     *flags = flags_;
//                 }
                return ObjectICommand( command );
            }

            case CMD_QUEUE_STOLEN_ITEM:
            {
                std::cout << "Item stolen . . . " << std::endl;
                return ObjectICommand( command );
            }

            default:
                LBUNIMPLEMENTED;
            }
        }
        catch (co::Exception& e)
        {
            LBWARN << e.what() << std::endl;
        }
        
        return ObjectICommand( 0, 0, 0, false );
}

SlaveFeedback StealingQueueSlave::sendSlaveFeedback()
{
    return     SlaveFeedback(*this, impl_->master_, impl_->masterInstanceID_);
}

void StealingQueueSlave::clear()
{
//     std::cout << "**************** StealingQueueSlave::clear()" << std::endl;
    impl_->queueLeft_.clear();
    impl_->queueRight_.clear();
}

void StealingQueueSlave::getInstanceData( co::DataOStream& os )
{
//     std::cout << "**************** StealingQueueSlave::getInstanceData( co::DataOStream& os )" << std::endl;
    os << getInstanceID() << getLocalNode()->getNodeID();
}

}
