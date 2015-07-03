
/* Copyright (c) 2014, David Steiner <steiner@ifi.uzh.ch>
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

#ifndef CO_PACKAGEDISTRIBUTOR_H
#define CO_PACKAGEDISTRIBUTOR_H

#include <co/api.h>
#include <co/types.h>

#include <lunchbox/referenced.h>

#include <string>

namespace lunchbox { class PerfLogger; }

namespace co
{

class PackageDistributor : public lunchbox::Referenced
{
public:
    virtual ~PackageDistributor() {}

    virtual void pushItem( QueueItem &item ) = 0;

    virtual void cmdSlaveFeedback( co::ICommand& ) {}

    virtual bool cmdGetItem( co::ICommand& ) { return false; }

    virtual void notifyQueueEnd() {}

    virtual void clear() {}
    
    virtual void setSlaveNodes( const co::Nodes& ) {}

    virtual void setPerfLogger( co::PerfLogger* ) {}
};

} // co

#endif // CO_PACKAGEDISTRIBUTOR_H
