
/* Copyright (c) 2013-2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "slaveFeedback.h"

#include <lunchbox/buffer.h>
#include <co/stealingQueueCommand.h>
#include <co/objectOCommand.h>

namespace co
{
namespace detail
{
class SlaveFeedback
{
public:
    SlaveFeedback( co::Object &object, NodePtr master, uint32_t masterInstanceID )
        : _object( object )
        , _master( master )
        , _masterInstanceID( masterInstanceID )
    {}
    
    SlaveFeedback( const SlaveFeedback& rhs )
        : _object( rhs._object )
        , _master( rhs._master )
        , _masterInstanceID( rhs._masterInstanceID )
    {}

    co::Object &_object;
    NodePtr _master;
    uint32_t _masterInstanceID;
};
}

SlaveFeedback::SlaveFeedback( co::Object &object, NodePtr master, uint32_t masterInstanceID )
    : DataOStream()
    , _impl( new detail::SlaveFeedback( object, master, masterInstanceID ))
{
    enableSave();
    _enable();
}

SlaveFeedback::SlaveFeedback( const SlaveFeedback& rhs )
    : DataOStream()
    , _impl( new detail::SlaveFeedback( *rhs._impl ))
{
    enableSave();
    _enable();
}

SlaveFeedback::~SlaveFeedback()
{
    lunchbox::Bufferb &buffer = getBuffer();
    _impl->_object.send( _impl->_master, CMD_QUEUE_SLAVE_FEEDBACK, _impl->_masterInstanceID )
        << Array< const void >( buffer.getData(), buffer.getSize( ));
    disable();
    delete _impl;
}

}
