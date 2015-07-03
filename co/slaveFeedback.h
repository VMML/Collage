
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

#ifndef CO_SLAVEFEEDBACK_H
#define CO_SLAVEFEEDBACK_H

#include <co/dataOStream.h> // base class
#include <co/types.h>

namespace co
{
namespace detail { class SlaveFeedback; }

class SlaveFeedback : public DataOStream
{
public:
    /** Destruct this queue item. @version 1.0 */
    CO_API ~SlaveFeedback();
    
    SlaveFeedback( co::Object &object, NodePtr master, uint32_t masterInstanceID );
    SlaveFeedback( const SlaveFeedback& rhs );

private:

    void sendData( const void*, const uint64_t, const bool ) override
        { LBDONTCALL }

    detail::SlaveFeedback* const _impl;
};

}

#endif // CO_SLAVEFEEDBACK_H
