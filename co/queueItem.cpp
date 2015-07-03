
/* Copyright (c) 2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *               2013-2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "queueItem.h"
#include <lunchbox/atomic.h>

#include "queueMaster.h"


namespace co
{
namespace detail
{

static lunchbox::a_ssize_t nPackages;
class QueueItem
{
public:
    QueueItem( co::Producer& queueMaster_ )
        : number( nPackages++ )
        , positionHint( 0 )
        , queueMaster( queueMaster_ )
    {}

    QueueItem( const QueueItem& rhs )
        : number( nPackages++ )
        , positionHint( 0 )
        , queueMaster( rhs.queueMaster )
    {}

    size_t number;
    float positionHint;
    co::Producer& queueMaster;
};
}

QueueItem::QueueItem( Producer& master )
    : DataOStream()
    , _impl( new detail::QueueItem( master ))
{
    enableSave();
    _enable();
}

QueueItem::QueueItem( const QueueItem& rhs )
    : DataOStream()
    , _impl( new detail::QueueItem( *rhs._impl ))
{
    enableSave();
    _enable();
}

QueueItem::~QueueItem()
{
    _impl->queueMaster._addItem( *this );
    disable();
    delete _impl;
}

QueueItem& QueueItem::setPositionHint( float posHint )
{
    _impl->positionHint = posHint;
    
    return *this;
}

size_t QueueItem::getNumber()
{
    return _impl->number;
}

float QueueItem::getPositionHint()
{
    return _impl->positionHint;
}

}
